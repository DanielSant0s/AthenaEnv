let test = Archive.open("OPNPS2LD-v1.1.0.zip");
let files = Archive.list(test);
console.log(JSON.stringify(files));
Archive.close(test);

Archive.untar("Enceladus.tar.gz");

System.sleep(99999);