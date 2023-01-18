<br />
<p align="center">
  <a href="https://github.com/DanielSant0s/AthenaEnv/">
    <img src="https://user-images.githubusercontent.com/47725160/151896523-5062975b-861d-434c-92df-3ead8fc85be2.png" alt="Logo" width="100%" height="auto">
  </a>

  <p align="center">
    Enhanced JavaScript environment for PlayStation 2™
    <br />
  </p>
</p>  


<details open="open">
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-athenaenv">About AthenaEnv</a>
      <ul>
        <li><a href="#function-types">Function types</a></li>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#coding">Coding</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#features">Features</a></li>
      </ul>
    </li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#thanks">Thanks</a></li>
  </ol>
</details>

## About AthenaEnv

AthenaEnv is a project that seeks to facilitate and at the same time brings a complete kit for users to create homebrew software for PlayStation 2 using the JavaScript language. It has dozens of built-in functions, both for creating games and apps. The main advantage over using AthenaEnv project instead of the pure PS2SDK is above all the practicality, you will use one of the simplest possible languages to create what you have in mind, besides not having to compile, just script and test, fast and simple.

### Modules:
* System: Files, folders and system stuff.
* Image: Image drawing.
* Draw: Shape drawing, triangles, circles etc.
* Render: Basic 3D support.
* Screen: The entire screen of your project (2D and 3D), being able to change the resolution, enable or disable parameters.
* Font: Functions that control the texts that appear on the screen, loading texts, drawing and unloading from memory.
* Pads: Above being able to draw and everything else, A human interface is important. Supports rumble and pressure sensitivity.
* Timer: Control the time precisely in your code, it contains several timing functions.
* Sound: Basic sound functions.
* Network: Net basics and web requests :D.
* Socket: Well, sockets.

New types are always being added and this list can grow a lot over time, so stay tuned.

### Built With

* [PS2DEV](https://github.com/ps2dev/ps2dev)
* [QuickJS](https://bellard.org/quickjs/)

## Coding

In this section you will have some information about how to code using AthenaEnv, from prerequisites to useful functions and information about the language.

### Prerequisites

Using AthenaEnv you only need one way to code and one way to test your code, that is, if you want, you can even create your code on PS2, but I'll leave some recommendations below.

* PC: [Visual Studio Code](https://code.visualstudio.com)(with JavaScript extension) and [PCSX2](https://pcsx2.net/download/development/dev-windows.html)(1.7.0 or above, enabled HostFS is required) or PS2Client for test.
* How to enable HostFS on PCSX2 1.7.0:  
![image](https://user-images.githubusercontent.com/47725160/145600021-b07dd873-137d-4364-91ec-7ace0b1936e2.png)

* Android: [QuickEdit](https://play.google.com/store/apps/details?id=com.rhmsoft.edit&hl=pt_BR&gl=US) and AetherSX2(HostFS is required) or a PS2 with uLE for test.

Oh, and I also have to mention that an essential prerequisite for using AthenaEnv is knowing how to code in JavaScript.

### Features

AthenaEnv uses a slightly modified version of the QuickJS interpreter for JavaScript language, which means that it brings almost modern JavaScript features so far.

**Float32**

This project introduces a (old)new data type for JavaScript: single floats. Despite being less accurate than the classic doubles for number semantics, they are important for performance on the PS2, as the console only processes 32-bit floats on its FPU.

You can write single floats on AthenaEnv following the syntax below:
```js
let test_float = 15.0f; // The 'f' suffix makes QuickJS recognizes it as a single float.
```

Below is the list of usable functions of AthenaEnv project currently, this list is constantly being updated.

P.S.: *Italic* parameters refer to optional parameters

### Image Module  

Construction:  

* var image = new Image(path, *mode*, *async_list*);  
  path - Path to the file, E.g.: "images/test.png".  
  *mode* - Choose between storing the image between **RAM** or **VRAM**, default value is RAM.  
  *async_list* - Gets a ImageList object, which is a asynchronous image loading list, if you want to load images in the background.  
```js
var test = new Image("owl.png", VRAM); 
``` 

Properties:

* width, height - Image drawing size, default value is the original image size.
* startx, starty - Beginning of the area that will be drawn from the image, the default value is 0.0.
* endx, endy - End of the area that will be drawn from the image, the default value is the original image size.
* angle - Define image rotation angle, default value is 0.0.
* color - Define image tinting, default value is Color.new(255, 255, 255, 128).
* filter - Choose between **LINEAR** or **NEAREST**, default value is NEAREST.  

Methods:

* draw(x, y) - Draw loaded image onscreen(call it every frame). Example: image.draw(15.0, 100.0);
* ready() - Returns true if an asynchronous image was successfully loaded in memory. 
```js
var loaded = image.ready();  
```

**ImageList**

Construction:

```js
var async_list = new ImageList(); // This constructor creates a new thread and a queue to load images in background, avoid building multiple ImageList objects.
```
Methods:

* process() - This method starts the thread and loads added images on the queue. 
```js
async_list.process();
```
  
  
### Draw module
* Draw.point(x, y, color)
* Draw.rect(x, y, width, height, color)
* Draw.line(x, y, x2, y2, color)
* Draw.circle(x, y, radius, color, *filled*)
* Draw.triangle(x, y, x2, y2, x3, y3, color, *color2*, *color3*)
* Draw.quad(x, y, x2, y2, x3, y3, x4, y4 color, *color2*, *color3*, *color4*)

### Render module

• Remember to enable zbuffering on screen mode, put the line of code below  
• Default NTSC mode(3D enabled): 
```js
Display.setMode(NTSC, 640, 448, CT24, INTERLACED, FIELD, true, Z16S);
```

* Render.init(aspect) *default aspect is 4/3, widescreen is 16/9
* var model = Render.loadOBJ(path, *texture*)
* Render.drawOBJ(model, pos_x, pos_y, pos_z, rot_x, rot_y, rot_z)
* Render.freeOBJ(model)  

**Camera**   
* Camera.position(x, y, z)
* Camera.rotation(x, y, z)

**Lights**  
* Lights.create(count)
* Lights.set(light, dir_x, dir_y, dir_z, r, g, b, type)  
  • Avaiable light types: AMBIENT, DIRECTIONAL  

### Screen module
* Screen.clear(*color*)
* Screen.flip()
* var freevram = Screen.getFreeVRAM()
* var fps = Screen.getFPS(frame_interval)
* Screen.setVSync(bool)
* Screen.waitVblankStart()
* Screen.setMode(mode, width, height, colormode, interlace, field, zbuffering, zbuf_colormode)  
  • Default NTSC mode(3D disabled): Screen.setMode(NTSC, 640, 448, CT24, INTERLACED, FIELD)  
  • Default NTSC mode(3D enabled):  Screen.setMode(NTSC, 640, 448, CT24, INTERLACED, FIELD, true, Z16S)  
  • Default PAL mode(3D disabled): Screen.setMode(PAL, 640, 512, CT24, INTERLACED, FIELD)  
  • Default PAL mode(3D enabled):  Screen.setMode(PAL, 640, 512, CT24, INTERLACED, FIELD, true, Z16S)  
  • Available modes: NTSC, _480p, PAL, _576p, _720p, _1080i  
  • Available colormodes: CT16, CT16S, CT24, CT32  
  • Available zbuffer colormodes: Z16, Z16S, Z24, Z32  
  • Available interlaces: INTERLACED, NONINTERLACED  
  • Available fields: FIELD, FRAME  

### Font module

Construction:  

```js
var font = new Font(path);  
```
  path - Path to a font file, E.g.: "images/atlas.png", "fonts/font.png".  
```js
var osdfnt = new Font();  //Load BIOS font  
var font = new Font("Segoe UI.ttf"); //Load trueType font 
``` 

Properties:
* color - Define font tinting, default value is Color.new(255, 255, 255, 128).

Methods:
* print(x, y, text) - Draw text on screen(call it every frame). Example: font.print(10.0, 10.0, "Hello world!));
* setScale(scale) - Set font scale;

### Pads module

* var pad = Pads.get(*port*)
  • pad.btns - Buttons  
  • pad.lx - Left analog horizontal position (left = -127, default = 0, right = 128)  
  • pad.ly - Left analog vertical position (up = -127, default = 0, down = 128)  
  • pad.rx - Right analog horizontal position (left = -127, default = 0, right = 128)  
  • pad.ry - Right analog vertical position (up = -127, default = 0, down = 128)  
    
  ![analog_graph](https://user-images.githubusercontent.com/47725160/154816009-99d7e5da-badf-409b-9a3b-3618fd372f09.png)

* var type = Pads.getType(*port*)
  • Pads.DIGITAL  
  • Pads.ANALOG  
  • Pads.DUALSHOCK  
* var press = Pads.getPressure(*port*, button)
* Pads.rumble(port, big, small)
* var ret = Pads.check(pad, button)
* Buttons list:  
  • Pads.SELECT  
  • Pads.START  
  • Pads.UP  
  • Pads.RIGHT  
  • Pads.DOWN  
  • Pads.LEFT  
  • Pads.TRIANGLE  
  • Pads.CIRCLE  
  • Pads.CROSS  
  • Pads.SQUARE  
  • Pads.L1  
  • Pads.R1  
  • Pads.L2  
  • Pads.R2  
  • Pads.L3  
  • Pads.R3  

### System module

* var fd = System.openFile(path, type)
* Types list:  
  • System.FREAD   
  • System.FWRITE  
  • System.FCREATE  
  • System.FRDWR  
* var buffer = System.readFile(file, size)
* System.writeFile(fd, data, size)
* System.closeFile(fd)
* System.seekFile(fd, pos, type)
* Types list:  
  • System.SET  
  • System.CUR  
  • System.END  
* var size = System.sizeFile(fd)
* System.doesFileExist(path)
* System.CurrentDirectory(path) *if path given, it sets the current dir, else it gets the current dir
* var listdir = System.listDir(*path*)
  • listdir[index].name - return file name on indicated index(string)  
  • listdir[index].size - return file size on indicated index(integer)  
  • listdir[index].directory - return if indicated index is a file or a directory(bool)  
* System.createDirectory(path)
* System.removeDirectory(path)
* System.removeFile(path)
* System.copyFile(source, dest)
* System.moveFile(source, dest)
* System.rename(source, dest)
* System.sleep(sec)
* var freemem = System.getFreeMemory()
* System.exitToBrowser()
* var info = System.getMCInfo(slot)  
  • info.type  
  • info.freemem  
  • info.format  
  
Asynchronous functions:  
* System.threadCopyFile(source, dest)
* var progress = System.getFileProgress()  
  • progress.current  
  • progress.final  

### Timer module

* var timer = Timer.new()
* Timer.getTime(timer)
* Timer.setTime(src, value)
* Timer.destroy(timer)
* Timer.pause(timer)
* Timer.resume(timer)
* Timer.reset(timer)
* Timer.isPlaying(timer)

### Sound module

* Sound.setFormat(bitrate, freq, channels)
* Sound.setVolume(volume)
* Sound.setADPCMVolume(channel, volume)
* var audio = Sound.loadADPCM(path)
* Sound.playADPCM(channel, audio)

### Network module

* Network.init(*ip*, *netmask*, *gateway*, *dns*)  
```js
Network.init("192.168.0.10", "255.255.255.0", "192.168.0.1", "192.168.0.1"); //Static mode  
Network.init(); //DHCP Mode, dynamic.  
```

* var conf = Network.getConfig()  
  Returns conf.ip, conf.netmask, conf.gateway, conf.dns.
  
 * Network.get(address)
 * Network.post(address, query)
  
* Network.deinit()  
  Shutdown network module.
  
### Socket module

Construction:  

* var s = new Socket(domain, type)  
```js
var s = new Socket(AF_INET, SOCK_STREAM);
```

Methods:

* connect(host, port)
* bind(host, port)
* listen()
* send(data) - Send data with Buffer
* recv(size) - Receive data to a buffer
* close()

## Contributing

Contributions are what make the open source community such an amazing place to be learn, inspire, and create. Any contributions you make are **greatly appreciated**.

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AwesomeFeature`)
3. Commit your Changes (`git commit -m 'Add some AwesomeFeature'`)
4. Push to the Branch (`git push origin feature/AwesomeFeature`)
5. Open a Pull Request

## License

Distributed under GNU GPL-3.0 License. See `LICENSE` for more information.

<!-- CONTACT -->
## Contact

Daniel Santos - [@danadsees](https://twitter.com/danadsees) - danielsantos346@gmail.com

Project Link: [https://github.com/DanielSant0s/AthenaEnv](https://github.com/DanielSant0s/AthenaEnv)

## Thanks

Here are some direct and indirect thanks to the people who made the project viable!

* guiprav - for 3dcb-duktape, which was the inspiration for bringing Enceladus and JavaScript together
* HowlingWolf&Chelsea - Tests, tips and many other things
* Whole PS2DEV team





