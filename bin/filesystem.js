function writeJSON(path, object){
    var fd = System.openFile(path, FCREATE);
    var content = JSON.stringify(object);
    System.writeFile(fd, content, content.length);
    System.closeFile(fd);
};

function readJSON(path){
    var fd = System.openFile(path, FREAD);
    var data = System.readFile(fd, System.sizeFile(fd));
    System.closeFile(fd);
    return JSON.parse(data);
};