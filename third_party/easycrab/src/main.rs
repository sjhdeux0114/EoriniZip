// EasyCrypt V2.x format implementation adapted from kjy00302/easycrab.
// EoriniZip changes: explicit exclusive output, checked lengths/reads,
// no password bypass, integrity failures are fatal, partial-output cleanup.
use clap::Parser;
use std::{fs::{self,File,OpenOptions},io::{self,Read,Write,Seek,SeekFrom,BufReader,BufWriter},path::PathBuf};
use aes::{Aes256Dec,cipher::{block_padding::Pkcs7,BlockDecryptMut,KeyIvInit,generic_array::GenericArray}};
use digest::Digest;
use sha1::Sha1;
use sha2::Sha512;
#[derive(Parser)]
struct Args { file:PathBuf, password:String, #[arg(long)] output:PathBuf }
fn invalid(message:&str)->io::Error { io::Error::new(io::ErrorKind::InvalidData,message) }
fn decrypt(args:&Args)->Result<(),Box<dyn std::error::Error>> {
    let mut input=BufReader::new(File::open(&args.file)?);
    let length=input.get_ref().metadata()?.len();
    if length<211 { return Err(invalid("Truncated EasyCrypt file").into()); }
    let mut magic=[0u8;7]; input.read_exact(&mut magic)?;
    if &magic[..3]!=b"EZC" || magic[3]!=2 { return Err(invalid("Unsupported EasyCrypt format/version").into()); }
    input.seek(SeekFrom::Start(0x43))?;
    let mut iv=[0u8;16]; let mut salt=[0u8;16];let mut hash=[0u8;64];
    input.read_exact(&mut iv)?;input.read_exact(&mut salt)?;input.read_exact(&mut hash)?;
    let mut password_hash=Sha512::new();password_hash.update(args.password.as_bytes());password_hash.update(salt);
    let actual=password_hash.finalize();
    if actual.iter().zip(hash).fold(0u8,|a,(x,y)|a|(x^y))!=0 {return Err(invalid("Incorrect password").into());}
    let data_length=length-0xA3-32;
    if data_length==0 || data_length%16!=0 {return Err(invalid("Invalid encrypted payload length").into());}
    let key=GenericArray::from_slice(&hash[..32]);
    input.seek(SeekFrom::End(-32))?;
    let mut checksum=[0u8;32];input.read_exact(&mut checksum)?;
    let expected=cbc::Decryptor::<Aes256Dec>::new(key,(&iv).into())
        .decrypt_padded_mut::<Pkcs7>(&mut checksum).map_err(|_|invalid("Invalid checksum padding"))?;
    if expected.len()!=20 {return Err(invalid("Invalid checksum length").into());}
    input.seek(SeekFrom::Start(0xA3))?;
    let mut cipher=cbc::Decryptor::<Aes256Dec>::new(key,(&iv).into());
    let output=OpenOptions::new().write(true).create_new(true).open(&args.output)?;
    let result=(||->Result<(),Box<dyn std::error::Error>>{
        let mut writer=BufWriter::new(output);
        let mut remaining=data_length;let mut buffer=[0u8;32768];let mut sha=Sha1::new();let mut last=101u64;
        while remaining>0{
            let size=remaining.min(buffer.len() as u64) as usize;
            input.read_exact(&mut buffer[..size])?;remaining-=size as u64;
            if remaining==0 {
                let plain=cipher.clone().decrypt_padded_mut::<Pkcs7>(&mut buffer[..size]).map_err(|_|invalid("Invalid data padding"))?;
                sha.update(plain);writer.write_all(plain)?;
            }else{
                for block in buffer[..size].chunks_exact_mut(16){cipher.decrypt_block_mut(GenericArray::from_mut_slice(block));}
                sha.update(&buffer[..size]);writer.write_all(&buffer[..size])?;
            }
            let percent=((data_length-remaining)*99/data_length).min(99);
            if percent!=last{println!("{}%",percent);last=percent;}
        }
        if sha.finalize().as_slice()!=expected {return Err(invalid("EasyCrypt integrity check failed").into());}
        writer.flush()?;writer.get_ref().sync_all()?;Ok(())
    })();
    if result.is_err(){let _=fs::remove_file(&args.output);}
    result
}
fn main(){
    let args=Args::parse();
    if let Err(e)=decrypt(&args){eprintln!("EasyCrypt: {e}");std::process::exit(1);}
    println!("100%");
}
