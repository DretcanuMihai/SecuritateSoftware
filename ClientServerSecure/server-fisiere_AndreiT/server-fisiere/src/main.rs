use std::{fmt::Display, io::ErrorKind, net::SocketAddr, path::PathBuf, sync::Arc, time::Duration};

use error_stack::{IntoReport, Report, ResultExt};
use serde::Deserialize;
use tokio::{
    fs::{File, OpenOptions},
    io::{AsyncRead, AsyncReadExt, AsyncWriteExt, BufWriter},
    net::{TcpListener, TcpStream},
};
#[repr(u8)]
#[derive(Debug, Clone, Copy)]
enum Response {
    Ok,
    PathInvalid,
    FileAlreadyExists,
    OperationAborted,
}

#[derive(Debug)]
enum MyError {
    CantRead,
    CantWrite,
    Other,
    InvalidParameter(Response),
}

impl Display for MyError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Something went wrong and got the response {:?}!", self)
    }
}

impl std::error::Error for MyError {}
async fn read_file_name(
    reader: &mut (impl AsyncRead + Unpin),
) -> error_stack::Result<String, MyError> {
    let size = reader
        .read_u8()
        .await
        .into_report()
        .change_context(MyError::CantRead)
        .attach_printable("Didn't manage to read the size of the path")?;
    if size == 0 {
        //TODO: What should I do?
        return Err(
            Report::new(MyError::InvalidParameter(Response::PathInvalid))
                .attach_printable("Got an invalid size of 0"),
        );
    }
    let mut buffer = vec![0u8; size as usize];
    reader
        .read_exact(&mut buffer)
        .await
        .into_report()
        .change_context(MyError::CantRead)
        .attach_printable("Didn't manage to read the path")?;

    let string = String::from_utf8(buffer)
        .into_report()
        .change_context(MyError::InvalidParameter(Response::PathInvalid))
        .attach_printable("Can't parse the path as a string")?;

    Ok(string)
}

async fn receiving_file(stream: &mut TcpStream, file: File) -> error_stack::Result<(), MyError> {
    let mut file = BufWriter::new(file);
    loop {
        let size = stream
            .read_u8()
            .await
            .into_report()
            .change_context(MyError::CantRead)?;
        if size == 0 {
            break;
        }
        let mut buffer = vec![0u8; size as usize];
        stream
            .read_exact(&mut buffer)
            .await
            .into_report()
            .change_context(MyError::CantRead)?;

        file.write_all(&buffer)
            .await
            .into_report()
            .change_context(MyError::InvalidParameter(Response::OperationAborted))?;

        stream
            .write_u8(Response::Ok as u8)
            .await
            .into_report()
            .change_context(MyError::CantWrite)?;
    }
    file.flush()
        .await
        .into_report()
        .change_context(MyError::InvalidParameter(Response::OperationAborted))?;
    stream
        .write_u8(Response::Ok as u8)
        .await
        .into_report()
        .change_context(MyError::CantWrite)?;
    Ok(())
}

async fn serve_client(
    mut stream: TcpStream,
    addr: &SocketAddr,
    config: &Config,
) -> error_stack::Result<(), MyError> {
    let result = try_serve_client(&mut stream, addr, &config).await;

    if let Err(ref e) = result {
        match e.current_context() {
            MyError::InvalidParameter(response) => {
                let response = *response as u8;
                stream
                    .write_u8(response as u8)
                    .await
                    .into_report()
                    .change_context(MyError::CantWrite)?;
                println!("Sended response {:?}", response);
            }
            MyError::CantRead => {}
            MyError::CantWrite => {}
            MyError::Other => {}
        }
    }

    result
}

async fn try_serve_client(
    stream: &mut TcpStream,
    addr: &SocketAddr,
    config: &Config,
) -> error_stack::Result<(), MyError> {
    let path = read_file_name(stream).await?;
    if path.contains("..") {
        return Err(
            Report::new(MyError::InvalidParameter(Response::PathInvalid)).attach_printable(
                format!(
                    "The path {} contains \"..\" and it's forbiden. Client {}",
                    path, addr
                ),
            ),
        );
    }
    let path = std::path::Path::new(&path);

    if !path.is_relative() {
        return Err(
            Report::new(MyError::InvalidParameter(Response::PathInvalid)).attach_printable(
                format!(
                    "The path {} is not relative and it's forbiden. Client {}",
                    path.display(),
                    addr
                ),
            ),
        );
    }

    let mut file_path = config.directory_path.clone();
    file_path.push(path);

    if !file_path.starts_with(&config.directory_path) {
        return Err(
            Report::new(MyError::InvalidParameter(Response::PathInvalid)).attach_printable(
                format!(
                    "The path {} is pointing outside of the directory and it's forbiden. Client {}",
                    path.display(),
                    addr
                ),
            ),
        );
    }

    println!(
        "Trying to write a file to the path {} by client {}",
        file_path.display(),
        addr
    );

    let file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .share_mode(0)
        .open(&file_path)
        .await;

    let file = match file {
        Ok(file) => file,
        Err(e) => {
            let response = if e.kind() == ErrorKind::AlreadyExists {
                Response::FileAlreadyExists
            } else {
                Response::PathInvalid
            };
            return Err(Report::from(e)
                .change_context(MyError::InvalidParameter(response))
                .attach_printable({
                    format!(
                        "Got an invalid file path: \"{}\" from {}",
                        file_path.display(),
                        addr
                    )
                }));
        }
    };

    // let file = file
    //     .into_report()
    //     .change_context(MyError::InvalidParameter(Response::FileAlreadyExists))
    //     .attach_printable_lazy(|| {
    //         format!(
    //             "Got an invalid file path: \"{}\" from {}",
    //             file_path.display(),
    //             addr
    //         )
    //     })?;

    stream
        .write_u8(Response::Ok as u8)
        .await
        .into_report()
        .change_context(MyError::CantWrite)?;

    println!("Client {} started sending a file to the server", addr);

    if let Err(e) = receiving_file(stream, file).await {
        tokio::fs::remove_file(&file_path)
            .await
            .into_report()
            .change_context(MyError::Other)
            .attach_printable(format!("Can't remove the file {}", file_path.display()))?;
        return Err(e);
    }
    println!("Client {} finished sending a file to the server!", addr);
    Ok(())
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct Config {
    directory_path: PathBuf,
    addr: SocketAddr,
    timeout: u64,
}

impl Default for Config {
    fn default() -> Config {
        Config {
            addr: "127.0.0.1:25565".parse().unwrap(),
            timeout: 30,
            directory_path: "server_directory".into(),
        }
    }
}

#[derive(Debug)]
struct MainError(String);

impl Display for MainError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl std::error::Error for MainError {}

#[tokio::main]
async fn main() -> error_stack::Result<(), MainError> {
    let mut config = match std::fs::File::open("config.yaml") {
        Ok(file) => serde_yaml::from_reader(file).unwrap_or_else(|_| {
            println!("Can't read the file \'config.yaml\" going back to default config");
            Config::default()
        }),
        Err(_) => {
            println!("Can't find the file config.yaml. Using default config");
            Config::default()
        }
    };

    println!("Using config {:#?}", config);

    if !config.directory_path.exists() {
        std::fs::create_dir_all(&config.directory_path)
            .into_report()
            .change_context(MainError(format!(
                "Can't create a directory at the location {}",
                config.directory_path.display()
            )))?;
    }

    config.directory_path = config
        .directory_path
        .canonicalize()
        .into_report()
        .change_context(MainError(format!(
            "Can't canonicalize path {}",
            config.directory_path.display()
        )))?;

    print!("Binding on address {}...", config.addr);
    let listener = TcpListener::bind(config.addr)
        .await
        .into_report()
        .change_context(MainError(format!(
            "Can't bind to the address {}",
            config.addr
        )))?;

    println!("Done");

    let config = Arc::new(config);

    loop {
        let (stream, addr) = listener
            .accept()
            .await
            .into_report()
            .change_context(MainError("Error at accept call".to_owned()))?;
        println!("Received a client with addr {}", addr);
        let config = Arc::clone(&config);
        tokio::spawn(async move {
            let duration = Duration::from_secs(config.timeout);
            let timeout_future =
                tokio::time::timeout(duration, serve_client(stream, &addr, &config));
            match timeout_future.await {
                Ok(result) => {
                    if let Err(e) = result {
                        println!("Error at client with address {}: {:?}", addr, e);
                    } else {
                        println!("Connection to client {} closed successfully", addr);
                    }
                }
                Err(_) => {
                    println!("Client {} timeout.", addr)
                }
            }
        });
    }
}
