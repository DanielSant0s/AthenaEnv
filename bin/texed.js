// {"name": "Text editor", "author": "Daniel Santos", "version": "06142023","file": "texed.js"}

var text = ""; // Armazena o texto digitado pelo usuário
var cursorX = 0; // Posição X do cursor
var cursorY = 0; // Posição Y do cursor

var mode = "text"; // Modo atual do editor ("text" ou "command")
var command = ""; // Comando digitado pelo usuário

const VK_BACKSPACE = 7;
const VK_ENTER = 10;

const VK_ALT_C = -126;
const VK_FUNCTION = 0;
const VK_ACTION = 27;
const VK_RIGHT = 41;
const VK_LEFT = 42;
const VK_DOWN = 43;
const VK_UP = 44;

const canvas = Screen.getMode();

const def_font = new Font("default");
def_font.scale = 0.6f;

var curFileName = "";

var showF1Text = true;

// Função para desenhar o editor na tela
function drawEditor() {
    Screen.clear(); // Limpa a tela
  
    // Divide o texto em linhas usando a quebra de linha como separador
    var lines = text.split("\n");
  
    // Desenha cada linha na tela
    for (var i = 0; i < lines.length; i++) {
      var line = lines[i];
      var linePosY = i * 16; // Posição Y da linha em pixels
      def_font.print(0, linePosY, line);
    }
  
    // Desenha a barra inferior
    def_font.print(0, canvas.height - 30, "Mode: " + mode + " | Command: " + command);
  
    // Calcula a posição X do cursor com base no tamanho da linha atual
    var currentLine = lines[cursorY];
    var cursorPosX = def_font.getTextSize(currentLine.substring(0, cursorX)).width;
  
    // Calcula a posição Y do cursor em pixels
    var cursorPosY = cursorY * 16;
  
    // Desenha o cursor na posição atual
    var cursorChar = "|"; // Caractere usado para representar o cursor
    def_font.print(cursorPosX, cursorPosY, cursorChar);

    if(showF1Text) {
        def_font.print((canvas.width/2)-def_font.getTextSize("Press F1 to enter on help menu.").width/2, (canvas.height/2)-13, "Press F1 to enter on help menu.");
    }
  
    Screen.flip(); // Atualiza a tela
  }
  
  // Função para remover um caractere na posição atual do cursor
  function removeCharacter() {
    // Divide o texto em linhas usando a quebra de linha como separador
    var lines = text.split("\n");
  
    // Verifica se a posição do cursor está dentro dos limites válidos
    if (cursorY >= 0 && cursorY < lines.length) {
      var line = lines[cursorY];
  
      // Verifica se a posição X do cursor está dentro dos limites válidos
      if (cursorX > 0 && cursorX <= line.length) {
        // Remove o caractere na posição atual do cursor
        lines[cursorY] = line.substring(0, cursorX - 1) + line.substring(cursorX);
  
        // Move o cursor para a esquerda
        cursorX--;
      }
    }
  
    // Atualiza o texto com as linhas modificadas
    text = lines.join("\n");
  }

var oldpad = undefined;
var pad = undefined;

var charCode = 0;
var oldCharCode = 0;

// Função para processar a entrada do usuário
function processInput() {
    oldpad = pad;
    pad = Pads.get(); // Obtém o estado do gamepad na porta 0

    oldCharCode = charCode;
    charCode = Keyboard.get();

    if(oldCharCode == VK_ACTION && charCode == VK_FUNCTION + 1) {
        while (true) {
            Screen.clear();
            oldCharCode = charCode;
            charCode = Keyboard.get();

            if(oldCharCode == VK_ACTION && charCode == VK_FUNCTION + 1) {
                break;
            }

            def_font.print(0, 0, "Press Alt+C to switch between command and text mode.");
            def_font.print(0, 16, "Press F1 to enter and exit help.");
            def_font.print(0, 48, "Commands:");
            def_font.print(0, 64, "new - Creates a new blank file");
            def_font.print(0, 80, "open file_name.txt - Open file");
            def_font.print(0, 96, "saveas file_name.txt - Save to new file");
            def_font.print(0, 112, "save - Saves the current file");
            Screen.flip();
        }
    }

    if(charCode !== 0) {
        console.log("Old char: " + oldCharCode);
        console.log("Char code: " + charCode);
    }
  
    // Verifica se está no modo de texto
    if (mode === "text") {
      // Converte o código ASCII do caractere para uma string

      if (charCode === VK_ALT_C) {
        mode = "command";
        command = "";
      } else if (oldCharCode == VK_ACTION && charCode == VK_UP) {
        cursorY--;
      } else if (oldCharCode == VK_ACTION && charCode == VK_DOWN) {
        cursorY++;
      } else if (oldCharCode == VK_ACTION && charCode == VK_LEFT) {
        if (cursorX > 0) {
          cursorX--;
        } else if (cursorY > 0) {
          cursorY--;
          var lines = text.split("\n");
          cursorX = lines[cursorY].length;
        }
      } else if (oldCharCode == VK_ACTION && charCode == VK_RIGHT) {
        var lines = text.split("\n");
        if (cursorX < lines[cursorY].length) {
          cursorX++;
        } else if (cursorY < lines.length - 1) {
          cursorY++;
          cursorX = 0;
        }
      } else if (charCode === VK_BACKSPACE) {
        if (cursorX > 0) {
          removeCharacter();
        } else if (cursorY > 0) {
          var lines = text.split("\n");
          if (lines[cursorY] === "" && cursorY !== 0) {
            // Remove a linha vazia
            lines.splice(cursorY, 1);
  
            // Move o cursor para a linha anterior
            cursorY--;
            cursorX = lines[cursorY].length;
  
            // Atualiza o texto com as linhas modificadas
            text = lines.join("\n");
          } else {
            // Remove o salto de linha
            var currentLine = lines[cursorY];
            var prevLine = lines[cursorY - 1];
  
            lines[cursorY - 1] = prevLine + currentLine;
            lines.splice(cursorY, 1);
  
            // Move o cursor para a posição correta
            cursorY--;
            cursorX = prevLine.length;
  
            // Atualiza o texto com as linhas modificadas
            text = lines.join("\n");
          }
        }
      } else if (charCode === VK_ENTER) {
        // Insere uma quebra de linha na posição atual do cursor
        var lines = text.split("\n");
        var line = lines[cursorY];
        lines[cursorY] = line.substring(0, cursorX) + "\n" + line.substring(cursorX);
  
        // Move o cursor para a próxima linha
        cursorY++;
        cursorX = 0;
  
        // Atualiza o texto com as linhas modificadas
        text = lines.join("\n");
      } else if (charCode !== 0 && charCode !== VK_ACTION) {
        var char = String.fromCharCode(charCode);
  
        // Insere o caractere na posição atual do cursor
        var lines = text.split("\n");
        var line = lines[cursorY];
        lines[cursorY] = line.substring(0, cursorX) + char + line.substring(cursorX);
  
        // Move o cursor para a direita
        cursorX++;
  
        // Atualiza o texto com as linhas modificadas
        text = lines.join("\n");
      }

    } else if (mode === "command") {
      // Verifica se o usuário pressionou o botão Triangle para voltar ao modo de texto
      if (charCode === VK_ALT_C) {
        mode = "text";
      } else if (charCode === VK_ENTER) {
        if (command.startsWith("saveAs ")) {
            var fileName = command.substring(5); // Obtém o nome do arquivo a ser salvo
            saveToFile(fileName); // Chama a função para salvar o arquivo
        } else if (command == "save") {
            saveToFile(curFileName); // Chama a função para salvar o arquivo
        } else if (command.startsWith("open ")) {
            var fileName = command.substring(5); // Obtém o nome do arquivo a ser salvo
            openFile(fileName); // Chama a função para salvar o arquivo
            cursorX = 0;
            cursorY = 0;
        }  else if (command == "new") {
            text = "";
            cursorX = 0;
            cursorY = 0;
        }

        command = ""; // Limpa o comando
      } else if (charCode === VK_BACKSPACE) {
        command = command.slice(0, -1);
      } else if (charCode !== 0) {
        var char = String.fromCharCode(charCode);
        command += char;
      }
    }
}


// Função para salvar o texto em um arquivo
function saveToFile(fileName) {
    var file = std.open(fileName, "w");
    file.write(Uint8Array.from(Array.from(text).map(letter => letter.charCodeAt(0))).buffer, 0, text.length);
    file.close();
}

function openFile(fileName) {
    cursorX = 0;
    cursorY = 0;
	curFileName = fileName;
    var file = std.open(fileName, "r");
    text = file.readAsString();
    file.close();
}

// Loop principal do editor
function mainLoop() {
  os.setInterval(() => {    
    processInput(); // Processa a entrada do usuário
    drawEditor(); // Desenha o editor na tela
  }, 16);
}

// Inicializa o editor
function initializeEditor() {
  IOP.loadDefaultModule(IOP.keyboard);
  Keyboard.init(); // Inicializa o teclado

  // Configura as propriedades do editor
  cursorX = 0;
  cursorY = 0;
  text = "";

  // Inicia o loop principal do editor
  mainLoop();
}

initializeEditor();

os.setTimeout(() => {   
    showF1Text = false;
}, 5000);
