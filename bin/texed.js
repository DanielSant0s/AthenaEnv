// {"name": "Text editor", "author": "Daniel Santos", "version": "06142023","file": "texed.js"}

var clipboard = "";

const sample = 
`const def_font = new Font("default"); // Loads Athena default font into memory.

let pad = undefined;

let main_loop = os.setInterval(() => {
	pad = Pads.get();
	Screen.clear(); // Fill the whole screen using the black color.
	
	def_font.print(0, 0, "Hello, World!"); // Prints Hello, World! at 0, 0 using Athena default font.
	
	if (Pads.check(pad, Pads.TRIANGLE)) {
		os.clearInterval(main_loop);
	}
	
	Screen.flip(); // Jump to the next frame.
}, 1);
`;

var text = ""; // Armazena o texto digitado pelo usu�rio
var cursorX = 0; // Posi��o X do cursor
var cursorY = 0; // Posi��o Y do cursor
var scrollX = 0;
var scrollY = 0; // Posição Y do scroll

var mode = "text"; // Modo atual do editor ("text" ou "command")
var command = ""; // Comando digitado pelo usu�rio

const VK_BACKSPACE = 7;
const VK_ENTER = 10;

const VK_FUNCTION = 0;
const VK_ACTION = 27;
const VK_RIGHT = 41;
const VK_LEFT = 42;
const VK_DOWN = 43;
const VK_UP = 44;
const VK_PAGE_UP = 37;
const VK_PAGE_DOWN = 40;

const canvas = Screen.getMode();

const def_font = new Font("default");
def_font.scale = Math.fround(0.6);

var curFileName = "";

var showHelpText = true;

var selectionMode = false;
var selectionStartX = 0;
var selectionStartY = 0;

const trans_pink = Color.new(255, 0, 255, 32);
const trans_white = Color.new(255, 255, 255, 32);

var selectionRectangles = []; // Array para armazenar as informações dos retângulos de seleção
function drawSelectionRectangles() {
	if (selectionMode) {
		// Limpa o array de retângulos de seleção
		selectionRectangles = [];
		
		// Divide o texto em linhas usando a quebra de linha como separador
		var lines = text.split("\n");
		
		// Verifica a posição inicial e final da seleção
		var startY = Math.min(selectionStartY, cursorY);
		var endY = Math.max(selectionStartY, cursorY);
		var startX, endX;
		
		if (startY === endY) {
			// Se a seleção estiver em uma única linha
			startX = Math.min(selectionStartX, cursorX);
			endX = Math.max(selectionStartX, cursorX);
		} else {
			// Se a seleção abranger várias linhas
			startX = (startY === selectionStartY) ? selectionStartX : cursorX;
			endX = (endY === selectionStartY) ? selectionStartX : cursorX;
		}
	  
		// Itera sobre as linhas para calcular as posições dos retângulos de seleção
		for (var i = startY; i <= endY; i++) {
			var line = lines[i];

			var startXPos, startYPos, endXPos, endYPos;

			if (i === startY) {
				// Primeira linha da seleção
				startXPos = def_font.getTextSize(line.substring(0, startX)).width;
				startYPos = ((i - scrollY) * 16) + 10;
				endXPos = (i === endY) ? def_font.getTextSize(line.substring(0, endX)).width : def_font.getTextSize(line).width;
				endYPos = ((i - scrollY + 1) * 16) + 10;
			} else if (i === endY) {
				// Última linha da seleção
				startXPos = 0;
				startYPos = ((i - scrollY) * 16) + 10;
				endXPos = def_font.getTextSize(line.substring(0, endX)).width;
				endYPos = ((i - scrollY + 1) * 16) + 10;
			} else {
				// Linhas intermediárias
				startXPos = 0;
				startYPos = ((i - scrollY) * 16) + 10;
				endXPos = def_font.getTextSize(line).width;
				endYPos = ((i - scrollY + 1) * 16) + 10;
			}
		  
			// Adiciona as informações do retângulo de seleção ao array
			selectionRectangles.push({ startX: startXPos, startY: startYPos, endX: endXPos, endY: endYPos });
		}
	}
}

// Função para desenhar o editor na tela
function drawEditor() {
	Screen.clear(); // Limpa a tela

	Draw.rect(0, canvas.height - 24, canvas.width, 24, trans_white);

	if (selectionMode) {
		drawSelectionRectangles();
		
		for (var i = 0; i < selectionRectangles.length; i++) {
			var rectangle = selectionRectangles[i];
			Draw.rect(rectangle.startX, rectangle.startY, rectangle.endX - rectangle.startX, rectangle.endY - rectangle.startY, trans_pink);
		}
	}

	// Divide o texto em linhas usando a quebra de linha como separador
	var lines = text.split("\n");

	// Calcula o número máximo de linhas visíveis na tela
	var maxVisibleLines = Math.floor((canvas.height - 30) / 16);

	// Ajusta a posição do scroll caso o cursor esteja fora da área visível
	if (cursorY < scrollY) {
		scrollY = cursorY;
	} else if (cursorY >= scrollY + maxVisibleLines) {
		scrollY = cursorY - maxVisibleLines + 1;
	}

	// Desenha as linhas visíveis na tela
	for (var i = 0; i < maxVisibleLines; i++) {
		var lineIndex = scrollY + i;
		if (lineIndex >= 0 && lineIndex < lines.length) {
			var line = lines[lineIndex];
			var linePosY = i * 16; // Posição Y da linha em pixels
			def_font.print(0, linePosY, line);
		}
	}

	// Desenha a barra inferior
	def_font.print(0, canvas.height - 30, "Mode: " + mode + " | Command: " + command);

	// Calcula a posição X do cursor com base no tamanho da linha atual
	var currentLine = lines[cursorY];
	if (currentLine !== undefined) {
		var cursorPosX = def_font.getTextSize(
		  currentLine.substring(0, cursorX)
		).width;
	}

	// Calcula a posição Y do cursor em pixels
	var cursorPosY = (cursorY - scrollY) * 16;

	// Desenha o cursor na posição atual
	var cursorChar = "|"; // Caractere usado para representar o cursor
	def_font.print(cursorPosX, cursorPosY, cursorChar);

	if (showHelpText) {
		def_font.print(
		  (canvas.width / 2) -
		      def_font.getTextSize("Press F1 to enter the help menu.")
		        .width /
		        2,
		  canvas.height / 2 - 13,
		  "Press F1 to enter the help menu."
		);
	}

	Screen.flip(); // Atualiza a tela
}
  
// Fun��o para remover um caractere na posi��o atual do cursor
function removeCharacter() {
    // Divide o texto em linhas usando a quebra de linha como separador
    var lines = text.split("\n");
  
    // Verifica se a posi��o do cursor est� dentro dos limites v�lidos
    if (cursorY >= 0 && cursorY < lines.length) {
    	var line = lines[cursorY];
  
    	// Verifica se a posi��o X do cursor est� dentro dos limites v�lidos
    	if (cursorX > 0 && cursorX <= line.length) {
    		// Remove o caractere na posi��o atual do cursor
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

// Fun��o para processar a entrada do usu�rio
function processInput() {
    oldpad = pad;
    pad = Pads.get(); // Obt�m o estado do gamepad na porta 0

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

            def_font.print(0, 0, "Press F2 to switch between command or text mode.");
            def_font.print(0, 16, "Press F1 to enter and exit help.");

            def_font.print(0, 48, "command mode:");
            def_font.print(0, 64, "new - Creates a new blank file");
            def_font.print(0, 80, "open file_name.txt - Open file");
            def_font.print(0, 96, "saveAs file_name.txt - Save to new file");
            def_font.print(0, 112, "save - Saves the current file");
            def_font.print(0, 128, "sample - Creates a minimal Athena sample");
            def_font.print(0, 144, "run filename.txt - Run the script using Athena Runtime");

            def_font.print(0, 176, "text mode:");
            def_font.print(0, 192, "Use arrows to navigate within the text.");
            def_font.print(0, 208, "Use Page Up and Page Down to scroll.");
            def_font.print(0, 224, "Press F3 to enter on select mode.");
            def_font.print(0, 240, "Press F6 to paste from clipboard.");

            def_font.print(0, 272, "select mode:");
            def_font.print(0, 288, "Use arrows to select inside the text.");
            def_font.print(0, 304, "F3 - Exit to text mode.");
            def_font.print(0, 320, "F4 - Copy to clipboard.");
            def_font.print(0, 336, "F5 - Cut to clipboard.");
            def_font.print(0, 352, "F6 - Paste from clipboard.");
            Screen.flip();
        }
    }
  
    // Verifica se est� no modo de texto
    if (mode === "text") {
    	// Converte o c�digo ASCII do caractere para uma string

    	if (oldCharCode == VK_ACTION && charCode == VK_FUNCTION + 3) {
    		// Ativa ou desativa o modo de seleção quando o usuário pressionar uma combinação de teclas específica (por exemplo, F3)
    		selectionMode = !selectionMode;
    		if (selectionMode) {
    		  // Armazena a posição inicial da seleção
    		  selectionStartX = cursorX;
    		  selectionStartY = cursorY;
    		}

    		mode = "select";
    	} else if (oldCharCode == VK_ACTION && charCode == VK_FUNCTION + 2) {
			mode = "command";
    	} else if (oldCharCode == VK_ACTION && charCode == VK_UP) {
    		if (cursorY > 0) {
    		  cursorY--;
    		}
    	} else if (oldCharCode == VK_ACTION && charCode == VK_DOWN) {
    		if (cursorY < text.split("\n").length-1) {
    		  cursorY++;
    		}
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
    	}  else if (oldCharCode == VK_ACTION && charCode == VK_PAGE_UP) {
    		// Move a visão do texto uma página para cima
    		if (scrollY > 0) {
    		  cursorY -= Math.floor((canvas.height - 30) / 16);
    		  scrollY -= Math.floor((canvas.height - 30) / 16);
    		}
    	} else if (oldCharCode == VK_ACTION && charCode == VK_PAGE_DOWN) {
    		// Move a visão do texto uma página para baixo
    		var maxLen = text.split("\n").length;
    		var scroll = Math.floor((canvas.height - 30) / 16);
    		if (scrollY < maxLen-scroll) {
    		  cursorY += scroll;
    		  scrollY += scroll;
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
  
    		    // Move o cursor para a posi��o correta
    		    cursorY--;
    		    cursorX = prevLine.length;
  
    		    // Atualiza o texto com as linhas modificadas
    		    text = lines.join("\n");
    		  }
    		}
    	} else if (charCode === VK_ENTER) {
    		// Insere uma quebra de linha na posi��o atual do cursor
    		var lines = text.split("\n");
    		var line = lines[cursorY];
    		lines[cursorY] = line.substring(0, cursorX) + "\n" + line.substring(cursorX);
  
    		// Move o cursor para a pr�xima linha
    		cursorY++;
    		cursorX = 0;
  
    		// Atualiza o texto com as linhas modificadas
    		text = lines.join("\n");
    	} else if (oldCharCode == VK_ACTION && charCode == VK_FUNCTION + 6) {
    		// Cola o texto da área de transferência quando o usuário pressionar uma combinação de teclas específica (por exemplo, F6)
			
    		if (clipboard !== "") {
    		  var lines = text.split("\n");
    		  var currentLine = lines[cursorY];
			
    		  // Insere o texto da área de transferência na posição atual
    		  lines[cursorY] = currentLine.substring(0, cursorX) + clipboard + currentLine.substring(cursorX);
			
    		  // Move o cursor para a direita
    		  cursorX += clipboard.length;
			
    		  // Atualiza o texto com as linhas modificadas
    		  text = lines.join("\n");
    		}
    	} else if (charCode !== 0 && charCode !== VK_ACTION) {
    		var char = String.fromCharCode(charCode);
  
    		// Insere o caractere na posi��o atual do cursor
    		var lines = text.split("\n");
    		var line = lines[cursorY];
    		lines[cursorY] = line.substring(0, cursorX) + char + line.substring(cursorX);
  
    		// Move o cursor para a direita
    		cursorX++;
  
    		// Atualiza o texto com as linhas modificadas
    		text = lines.join("\n");
    	}

    } else if (mode === "command") {
    	// Verifica se o usu�rio pressionou o bot�o Triangle para voltar ao modo de texto
    	if (oldCharCode == VK_ACTION && charCode == VK_FUNCTION + 2) {
    	  mode = "text";
    	} else if (charCode === VK_ENTER) {
    	  if (command.startsWith("saveAs ")) {
    	      var fileName = command.substring(7); // Obt�m o nome do arquivo a ser salvo
    	      saveToFile(fileName); // Chama a fun��o para salvar o arquivo
    	  } else if (command == "save") {
    	      saveToFile(curFileName); // Chama a fun��o para salvar o arquivo
    	  } else if (command.startsWith("open ")) {
    	      var fileName = command.substring(5); // Obt�m o nome do arquivo a ser salvo
    	      openFile(fileName); // Chama a fun��o para salvar o arquivo
    	      cursorX = 0;
    	      cursorY = 0;
    	  }  else if (command == "new") {
    	      text = "";
    	      cursorX = 0;
    	      cursorY = 0;
    	  } else if (command == "sample") {
    	    text = sample;
    	    cursorX = 0;
    	    cursorY = 0;
    	  } else if (command.startsWith("run ")) {
    	    var fileName = command.substring(4); // Obt�m o nome do arquivo a ser salvo
    	    System.loadELF(System.boot_path + "athena.elf", [fileName]); // Chama a fun��o para salvar o arquivo
    	}

    	  mode = "text";

    	  command = ""; // Limpa o comando
    	} else if (charCode === VK_BACKSPACE) {
    	  command = command.slice(0, -1);
    	} else if (charCode !== 0 && charCode !== VK_ACTION) {
    	  var char = String.fromCharCode(charCode);
    	  command += char;
    	}
    } else if (mode == "select") {
    	if (oldCharCode == VK_ACTION && charCode == VK_FUNCTION + 3) {
    	  mode = "text";
    	  selectionMode = false; // Desativa o modo de seleção ao sair do modo de seleção
    	} else if (oldCharCode == VK_ACTION && charCode == VK_UP) {
    	  if (cursorY > 0) {
    	    cursorY--;
    	  }
    	} else if (oldCharCode == VK_ACTION && charCode == VK_DOWN) {
    	  if (cursorY < text.split("\n").length - 1) {
    	    cursorY++;
    	  }
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
    	} else if (oldCharCode == VK_ACTION && charCode == VK_PAGE_UP) {
    	  // Move a visão do texto uma página para cima
    	  if (scrollY > 0) {
    	    cursorY -= Math.floor((canvas.height - 30) / 16);
    	    scrollY -= Math.floor((canvas.height - 30) / 16);
    	  }
    	} else if (oldCharCode == VK_ACTION && charCode == VK_PAGE_DOWN) {
    	  // Move a visão do texto uma página para baixo
    	  var maxLen = text.split("\n").length;
    	  var scroll = Math.floor((canvas.height - 30) / 16);
    	  if (scrollY < maxLen - scroll) {
    	    cursorY += scroll;
    	    scrollY += scroll;
    	  }
    	} else if (charCode == VK_FUNCTION + 4) {
    	  copytoClipboard();
    	} else if (charCode == VK_FUNCTION + 5) {
    		// Recortar texto selecionado quando o usuário pressionar uma combinação de teclas específica (por exemplo, F5)
    		if (selectionMode) {
    			copytoClipboard();
  
    			// Deleta o texto selecionado
    			var lines = text.split("\n");
    			var newText = "";
  
    			// Verifica se a seleção começa antes da posição atual ou depois
    			var startY = Math.min(selectionStartY, cursorY);
    			var endY = Math.max(selectionStartY, cursorY);
    			if (startY === endY) {
    			  // Mesma linha
    			  newText += lines[startY].substring(0, Math.min(selectionStartX, cursorX));
    			  newText += lines[startY].substring(Math.max(selectionStartX, cursorX));
    			} else {
    			  // Múltiplas linhas
    			  newText += lines[startY].substring(0, selectionStartX) + "\n";
    			  newText += lines[endY].substring(cursorX) + "\n";
    			  for (var i = 0; i < lines.length; i++) {
    			    if (i < startY || i > endY) {
    			      newText += lines[i] + "\n";
    			    }
    			  }
    			}
  
    		  	text = newText;
    		}
    	} else if (charCode == VK_FUNCTION + 6) {
    		// Colar texto da área de transferência (clipboard) quando o usuário pressionar uma combinação de teclas específica (por exemplo, F6)
    		// Insere o texto da área de transferência na posição atual
    		if (selectionStartX !== cursorX || selectionStartY !== cursorY) {
    			var lines = text.split("\n");
    			var newText = "";
				
    			// Verifica se a seleção começa antes da posição atual ou depois
    			var startY = Math.min(selectionStartY, cursorY);
    			var endY = Math.max(selectionStartY, cursorY);
    			if (startY === endY) {
    				// Mesma linha
    				newText += lines[startY].substring(0, Math.min(selectionStartX, cursorX));
    				newText += clipboard;
    				newText += lines[startY].substring(Math.max(selectionStartX, cursorX));
    			} else {
    				// Múltiplas linhas
    				newText += lines[startY].substring(0, selectionStartX) + clipboard + "\n";
    				newText += lines[endY].substring(cursorX) + "\n";
    				for (var i = 0; i < lines.length; i++) {
    					if (i < startY || i > endY) {
    					  	newText += lines[i] + "\n";
    					}
    				}
    			}
			  
    			text = newText;
    		}
    	}
    	
    }
}


// Fun��o para salvar o texto em um arquivo
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
		processInput(); // Processa a entrada do usu�rio
		drawEditor(); // Desenha o editor na tela
	}, 1);
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
    showHelpText = false;
}, 5000);