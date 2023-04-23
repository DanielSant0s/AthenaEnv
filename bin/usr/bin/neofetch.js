var ee_info = System.getCPUInfo();
var gs_info = System.getGPUInfo();

console.log(JSON.stringify(ee_info));

var logo = 
`
                                        
         #@@@@@@@@@@@@@@@*              ${user}@${device}
  /@@@&@@@@@@@@@@@@@#  *@*              ------------
     /@@@@@@@@@@@.  @@@@@@@             OS: AthenaEnv v2
    @@ #@@@@@@& ,@@,  @@@@@@            Kernel: PS2DEV Kernel
    /@@,*@@@@ .@@@/  (@@@@@@@           Uptime: null
   .     @@@*       *@@@@@@@@           Packages: 4 (dpkg)
   @@@@@@   #@@@@@@@@@@@@@@@@           Shell: owlsh 0.1
    @@@@@( *@@@@@@@@@@@@@@@@@@          Terminal: TermOwl
    .@@@@@@@@@@@@@@@ @@@@@@@@@@@.       CPU: MIPS R5900 impl. ${ee_info.implementation} rev. ${ee_info.revision}
    @@@(@@@@@@@@&   @@@@@@@@@@@@@@      GPU: Graphics Synthesizer id ${gs_info.id} rev. ${gs_info.revision}
   .@@@   @@@@     *@@@@@@@@@@@@@@@.    Memory: ${Math.floor(System.getMemoryStats().used / 1048576)}MiB / ${Math.floor(ee_info.RAMSize / 1048576)}MiB
   .@@@            /@@@@@@@@@@@@@@@@.   
    @@@,            @@@@@@@@@@@@@@@@@   
     @@@            /@@@@@@@@@@@@@@@@@  
      @@@.           /@@@@@@@@@@@@@@@@. 
        @@@            @@@@@@@@@@@@@@@@ 
          &@@,           @@@@@@@@@@@@@@ 
             %@@&          (@@@@@@@@@@@ 
                 *@@@@@*.      @@@@@@@@ 
                   *@@@.    #@@@    %@# 
                  ,@@@     /@@&         
        #@@@,@@@(@@@( @@%&@@@@@@@@*     
             /   %    #  .(                                                                      
`

//Console.setFontColor(0xFFFF0080);
Console.print(logo);

logo = null;
ee_info = null;
gs_info = null;
