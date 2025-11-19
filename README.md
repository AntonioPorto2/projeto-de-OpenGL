Este projeto em C++ utiliza FreeGLUT/OpenGL para simular uma cena de baliza 3D com um carro, cones e um plano de asfalto com textura procedural.
✨ DestaquesGráficos OpenGL: Carro, cones e chão renderizados em 3D.
Textura Procedural: O asfalto é gerado por algoritmo (ruído), sem uso de arquivos de imagem.Física Simplificada: 
O movimento do carro utiliza um modelo de bicicleta para calcular a rotação (direção) com base no ângulo de esterço e velocidade.
Câmera Livre: Câmera controlável (modo voo) para visualização da cena.
🛠️ Configuração RápidaO projeto requer um compilador C++ (GCC) e a biblioteca FreeGLUT.
➡️ Comando de CompilaçãoUse o seguinte comando no terminal:Bashg++ projeto.cpp -lfreeglut -lglu32 -lopengl32 -lgdi32 -o projeto.exe
➡️ ExecuçãoBash./projeto.exe
🕹️ ControlesAçãoTeclasDirigir CarroSetas (UP/DOWN para velocidade, LEFT/RIGHT para esterço)Mover CâmeraW/S/A/D (movimento horizontal)Ajustar Altura CâmeraQ/EResetar PosiçõesRSairESC
