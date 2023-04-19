
if (!args[0]){
    System.currentDir(System.boot_path)
} else{
    if (System.doesFileExist(args[0])){
        System.currentDir(args[0] + "/")
    }else{
        Console.print("cd: File or Directory Not Found: " + args[0] + "\n")
    }
    
}