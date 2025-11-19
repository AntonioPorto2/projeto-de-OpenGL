// projeto.cpp
//
// OBJETIVO: Simulação simples de baliza em 3D usando FreeGLUT/OpenGL.
//
// FUNCIONALIDADES:
// - Textura PROCEDURAL de asfalto (ruído + variação)
// - Corredor de cones para simular a baliza.
// - Controles:
//   - Setas (UP/DOWN/LEFT/RIGHT) controlam o carro (aceleração, freio, esterço).
//   - WASD/QE controlam a câmera (movimento livre tipo "voo" em XZ e altura Y).
//   - R: reseta a cena.
//   - ESC: sai da aplicação.
//
// COMANDO DE COMPILAÇÃO (GCC/MinGW):
// g++ projeto.cpp -lfreeglut -lglu32 -lopengl32 -lgdi32 -o projeto.exe

#include <GL/freeglut.h> // Inclui FreeGLUT e, indiretamente, OpenGL.
#include <cmath>         // Funções matemáticas (sinf, cosf, tanf, fabsf).
#include <cstdlib>       // Funções gerais (rand, srand).
#include <ctime>         // Funções de tempo (time) para inicialização do rand.
#include <vector>        // Uso do std::vector para armazenar as posições dos cones.
#include <iostream>      // Entrada/Saída padrão (não essencial, mas comum).
#include <cstdio>        // snprintf para formatar texto no HUD.
#include <string>        // Para uso de std::string no HUD.

// ----------------------- Configuração geral -----------------------
const int WIN_W = 1000;
const int WIN_H = 700;

// ----------------------- Câmera (controle WASD) --------------------
// Estrutura para armazenar o estado da câmera (posição e velocidade).
struct Camera {
    float x = 0.0f;
    float y = 3.2f; // Altura inicial
    float z = 10.0f;
    float speed = 6.0f; // unidades por segundo para movimento livre
} cam;

// Flags booleanas para rastrear o estado das teclas de movimento da câmera.
bool camForward=false, camBack=false, camLeft=false, camRight=false, camUp=false, camDown=false;

// ----------------------- Carro (setas) -----------------------------
// Estrutura para armazenar o estado físico do carro.
struct Car {
    float x=0.0f;
    float y=0.25f;      // y é a altura (meia altura do modelo do carro).
    float z=6.0f;
    float heading=180.0f; // Ângulo de rotação em Y (0=positivo Z, 90=positivo X, 180=negativo Z).
    float speed=0.0f;   // Velocidade atual do carro (pode ser positiva ou negativa).
    float wheelAngle=0.0f; // Ângulo de esterço das rodas dianteiras (em graus).
} car;

// Flags booleanas para rastrear o estado das teclas de controle do carro.
bool keyUp=false, keyDown=false, keyLeft=false, keyRight=false;

// Parâmetros físicos simplificados.
const float CAR_ACCEL = 6.0f;   // Aceleração em unidades/s^2 (CORRIGIDO: Renomeado de ACCEL).
const float BRAKE = 8.0f;       // Desaceleração por freio (não usado diretamente, mas pode ser usado).
const float FRICTION = 3.0f;    // Desaceleração por fricção/arrasto quando não há input.
const float MAX_SPEED = 8.0f;
const float MAX_REVERSE = -3.0f;
const float MAX_WHEEL_DEG = 30.0f;    // Ângulo máximo de esterço das rodas.
const float WHEEL_SPEED_DEG = 90.0f; // Velocidade de mudança do ângulo de esterço (graus/s).
const float WHEEL_BASE = 1.0f;      // Distância entre eixos (base para o modelo de bicicleta).
const float PI = 3.14159265f;

// ----------------------- Cones (formam corredor) -------------------
// Armazena as posições (x,z) dos cones.
std::vector<std::pair<float,float>> cones;
const float CORRIDOR_HALF_WIDTH = 1.2f; // Meia largura do corredor.
const int NUM_PAIRS = 3;                // Número de pares de cones.
const float PAIR_SPACING = 3.0f;        // Espaçamento em Z entre os pares.

// ----------------------- Textura procedural -----------------------
GLuint texAsphalt = 0;
const int TEX_SIZE = 256;

/**
 * Gera uma textura procedural simples que imita asfalto (grayscale noise + streaks).
 */
void generateAsphaltProc(unsigned char *buf, int size) {
    // Inicializa o gerador de números aleatórios apenas uma vez.
    static bool seeded = false;
    if(!seeded){ srand((unsigned)time(NULL)); seeded = true; }

    for(int y=0;y<size;y++){
        for(int x=0;x<size;x++){
            // Lógica para gerar tons de cinza com pequenas variações (ruído).
            int base = 40 + (rand() % 40);
            int noise = rand() % 40 - 20;
            // Efeito de 'manchas' ou 'riscos' que simulam o desgaste do asfalto.
            float streak = 0.0f;
            if ((x + y) % 37 < 8) streak = 8.0f * ((rand()%10)/10.0f);
            int v = base + noise + (int)streak;
            // Limita os valores entre 0 e 255.
            if(v<0) v=0; if(v>255) v=255;
            int idx = (y*size + x)*3;
            // Define o canal RGB para a cor cinza (tons iguais).
            buf[idx+0] = buf[idx+1] = buf[idx+2] = (unsigned char)v;
        }
    }
}

/**
 * Cria um ID de textura GL a partir de um buffer de dados RGB.
 */
GLuint createTextureFromBuffer(unsigned char *buf, int size) {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    // Envia os dados da imagem para a GPU.
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,size,size,0,GL_RGB,GL_UNSIGNED_BYTE,buf);
    // Configura os parâmetros da textura para suavização (LINEAR) e repetição (REPEAT).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return id;
}

// Inicializa a textura de asfalto.
void initTextures(){
    unsigned char *buf = new unsigned char[TEX_SIZE*TEX_SIZE*3];
    generateAsphaltProc(buf, TEX_SIZE);
    texAsphalt = createTextureFromBuffer(buf, TEX_SIZE);
    delete[] buf;
}

// ----------------------- Desenho de objetos -----------------------

/**
 * Desenha o chão usando a textura de asfalto.
 */
void drawGroundTextured() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texAsphalt);
    glColor3f(1.0f,1.0f,1.0f); // Garante que a cor base é branca para que a textura não seja escurecida.
    glBegin(GL_QUADS);
        glNormal3f(0,1,0); // Normal aponta para cima.
        // Mapeamento de textura: 6.0f faz a textura de 256x256 se repetir 6 vezes no chão.
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-12.0f, 0.0f, 16.0f);
        glTexCoord2f(6.0f, 0.0f); glVertex3f( 12.0f, 0.0f, 16.0f);
        glTexCoord2f(6.0f, 6.0f); glVertex3f( 12.0f, 0.0f, -24.0f);
        glTexCoord2f(0.0f, 6.0f); glVertex3f(-12.0f, 0.0f, -24.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

/**
 * Desenha um cone na posição (x, z) com altura Y=0.
 */
void drawConeAt(float x, float z) {
    glPushMatrix();
      glTranslatef(x, 0.0f, z);
      glColor3f(1.0f, 0.45f, 0.05f); // Cor laranja brilhante.
      glutSolidCone(0.22, 0.5, 16, 8); // Cone sólido.
    glPopMatrix();
}

/**
 * Desenha o modelo do carro (cubo + teto + 4 rodas torus).
 */
void drawCarModel(const Car &c) {
    glPushMatrix();
      // 1. Posiciona o carro no mundo e ajusta sua altura.
      glTranslatef(c.x, c.y + 0.25f, c.z);
      // 2. Rotaciona o carro em torno do eixo Y (heading)
      glRotatef(c.heading, 0,1,0);

      // Corpo (Body)
      glPushMatrix();
        glColor3f(0.15f, 0.25f, 0.9f); // Azul
        glScalef(1.1f, 0.5f, 1.8f);   // Escala para formato de carro
        glutSolidCube(1.0);
      glPopMatrix();

      // Teto (Roof/Cabin)
      glPushMatrix();
        glColor3f(0.8f, 0.9f, 0.95f); // Cor clara
        glTranslatef(0.0f, 0.35f, -0.1f);
        glScalef(0.7f, 0.3f, 0.6f);
        glutSolidCube(1.0);
      glPopMatrix();

      // Rodas (Wheels) - Usam Torus (donut shape)
      glColor3f(0.02f,0.02f,0.02f); // Preto
      const float wx=0.55f, wz=0.65f, wy=-0.25f; // Posições relativas das rodas

      // Frente Esquerda (com ângulo de esterço)
      glPushMatrix();
        glTranslatef(-wx, wy, -wz);
        glRotatef(c.wheelAngle, 0, 1, 0); // Rotação do esterço!
        glutSolidTorus(0.06,0.12,10,10);
      glPopMatrix();

      // Frente Direita (com ângulo de esterço)
      glPushMatrix();
        glTranslatef( wx, wy, -wz);
        glRotatef(c.wheelAngle, 0, 1, 0); // Rotação do esterço!
        glutSolidTorus(0.06,0.12,10,10);
      glPopMatrix();

      // Traseira Esquerda (sem ângulo de esterço)
      glPushMatrix();
        glTranslatef(-wx, wy, wz);
        glutSolidTorus(0.06,0.12,10,10);
      glPopMatrix();

      // Traseira Direita (sem ângulo de esterço)
      glPushMatrix();
        glTranslatef( wx, wy, wz);
        glutSolidTorus(0.06,0.12,10,10);
      glPopMatrix();

    glPopMatrix();
}

// ----------------------- Inicialização da cena --------------------

/**
 * Configura as posições dos cones para formar um corredor.
 */
void setupConesStraightCorridor() {
    cones.clear();
    float startZ = 2.0f;
    for(int i=0;i<NUM_PAIRS;i++){
        float z = startZ - i * PAIR_SPACING;
        cones.emplace_back(-CORRIDOR_HALF_WIDTH, z); // Esquerda
        cones.emplace_back( CORRIDOR_HALF_WIDTH, z); // Direita
    }
    // Adiciona um cone marcador extra para o fim da baliza.
    cones.emplace_back(0.0f, startZ - NUM_PAIRS * PAIR_SPACING - 1.5f);
}

// ----------------------- Luzes e GL states -------------------------

/**
 * Configura as luzes e o modelo de iluminação do OpenGL.
 */
void setupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL); // Permite que glColor altere as propriedades de material.
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    // Luz Ambiente (ilumina a cena uniformemente).
    GLfloat amb[] = {0.25f,0.25f,0.25f,1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);

    // Luz Direcional (simula o sol).
    GLfloat col[] = {1.0f,0.95f,0.85f,1.0f}; // Cor branca/amarelada
    GLfloat pos[] = {0.2f,1.0f,0.3f,0.0f};  // w=0 indica luz direcional.
    glLightfv(GL_LIGHT0, GL_DIFFUSE, col);
    glLightfv(GL_LIGHT0, GL_SPECULAR, col);
    glLightfv(GL_LIGHT0, GL_POSITION, pos);

    // Configura o material padrão para o brilho especular (para reflexos em cones/carro).
    GLfloat spec[] = {0.2f,0.2f,0.2f,1.0f};
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT, GL_SHININESS, 32.0f);
}

// ----------------------- Input handlers ---------------------------

/**
 * Função de callback chamada quando uma tecla regular (como WASD, ESC) é pressionada.
 * (Corrigido: Renomeado de keyDown para keyboardDown para evitar conflito).
 */
void keyboardDown(unsigned char key, int x, int y) {
    switch(key) {
        case 'w': camForward=true; break;
        case 's': camBack=true; break;
        case 'a': camLeft=true; break;
        case 'd': camRight=true; break;
        case 'q': camUp=true; break;
        case 'e': camDown=true; break;
        case 'r': // Reseta posições da câmera e do carro.
            car.x = 0; car.z = 6; car.heading=180; car.speed=0; car.wheelAngle=0;
            cam.x = 0; cam.y = 3.2f; cam.z = 10.0f;
            break;
        case 27: // ASCII 27 é ESC
            exit(0);
        default: break;
    }
}

/**
 * Função de callback chamada quando uma tecla regular é liberada.
 * (Corrigido: Renomeado de keyUp para keyboardUp para evitar conflito).
 */
void keyboardUp(unsigned char key, int x, int y) {
    switch(key) {
        case 'w': camForward=false; break;
        case 's': camBack=false; break;
        case 'a': camLeft=false; break;
        case 'd': camRight=false; break;
        case 'q': camUp=false; break;
        case 'e': camDown=false; break;
        default: break;
    }
}

/**
 * Função de callback chamada quando uma tecla especial (como setas) é pressionada.
 */
void specialDown(int key, int x, int y) {
    switch(key) {
        case GLUT_KEY_UP: keyUp=true; break;      // Acelerar
        case GLUT_KEY_DOWN: keyDown=true; break;  // Ré/Freio
        case GLUT_KEY_LEFT: keyLeft=true; break;  // Virar esquerda
        case GLUT_KEY_RIGHT: keyRight=true; break; // Virar direita
    }
}
/**
 * Função de callback chamada quando uma tecla especial é liberada.
 */
void specialUp(int key, int x, int y) {
    switch(key) {
        case GLUT_KEY_UP: keyUp=false; break;
        case GLUT_KEY_DOWN: keyDown=false; break;
        case GLUT_KEY_LEFT: keyLeft=false; break;
        case GLUT_KEY_RIGHT: keyRight=false; break;
    }
}

// ----------------------- Physics & update ------------------------

int prevTime = 0; // Armazena o tempo da última atualização.

/**
 * Atualiza o estado físico da câmera e do carro. Chamada a cada frame (timerFunc).
 * @param dt Tempo decorrido desde o último frame (em segundos).
 */
void updatePhysics(float dt) {
    // --- Lógica da Câmera ---
    float cs = cam.speed * dt;
    // Movimento simples nos eixos globais X e Z.
    if(camForward) { cam.z -= cs; }
    if(camBack) { cam.z += cs; }
    if(camLeft) { cam.x -= cs; }
    if(camRight) { cam.x += cs; }
    // Movimento vertical.
    if(camUp) { cam.y += cs; }
    if(camDown) { cam.y -= cs; if(cam.y < 0.5f) cam.y = 0.5f; } // Limita a altura mínima.

    // --- Lógica do Carro ---

    // 1. Controle do Ângulo de Esterço (Rodas Dianteiras)
    if(keyLeft) car.wheelAngle += WHEEL_SPEED_DEG * dt;
    else if(keyRight) car.wheelAngle -= WHEEL_SPEED_DEG * dt;
    else {
        // Auto-centralização do volante (lento).
        if(car.wheelAngle > 1.0f) car.wheelAngle -= 60.0f*dt;
        else if(car.wheelAngle < -1.0f) car.wheelAngle += 60.0f*dt;
        else car.wheelAngle = 0.0f;
    }
    // Limita o ângulo de esterço.
    if(car.wheelAngle > MAX_WHEEL_DEG) car.wheelAngle = MAX_WHEEL_DEG;
    if(car.wheelAngle < -MAX_WHEEL_DEG) car.wheelAngle = -MAX_WHEEL_DEG;

    // 2. Controle da Velocidade (Aceleração e Freio/Fricção)
    if(keyUp) car.speed += CAR_ACCEL * dt;  // Acelera
    else if(keyDown) car.speed -= CAR_ACCEL * dt; // Ré ou freio
    else {
        // Fricção/Arrasto: Se não estiver acelerando, a velocidade cai.
        if(car.speed > 0.0f) { car.speed -= FRICTION * dt; if(car.speed < 0) car.speed = 0.0f; }
        else if(car.speed < 0.0f) { car.speed += FRICTION * dt; if(car.speed > 0) car.speed = 0.0f; }
    }
    // Limita a velocidade máxima.
    if(car.speed > MAX_SPEED) car.speed = MAX_SPEED;
    if(car.speed < MAX_REVERSE) car.speed = MAX_REVERSE;

    // 3. Atualização da Posição e Direção (Modelo de Bicicleta Simplificado)
    float steerRad = car.wheelAngle * (PI/180.0f); // Converte esterço para radianos.
    if(fabsf(steerRad) > 1e-4f) {
        // R = Distância entre eixos / tan(ângulo de esterço) -> Raio de Curva
        float R = WHEEL_BASE / tanf(steerRad);
        // Calcula a velocidade angular (deg/s).
        float angVel = (car.speed / R) * (180.0f/PI);
        car.heading += angVel * dt; // Atualiza o ângulo de direção.
    }
    // Move o carro na direção atual (heading).
    float hr = car.heading * (PI/180.0f); // Converte heading para radianos.
    car.x += sinf(hr) * car.speed * dt;  // Movimento X = sin(heading) * velocidade * dt
    car.z += cosf(hr) * car.speed * dt;  // Movimento Z = cos(heading) * velocidade * dt

    // 4. Limites de Borda (opcional)
    if(car.x > 10.0f) car.x = 10.0f;
    if(car.x < -10.0f) car.x = -10.0f;
    if(car.z > 16.0f) car.z = 16.0f;
    if(car.z < -24.0f) car.z = -24.0f;
}

// ----------------------- HUD (Head-Up Display) ------------------------------------

/**
 * Desenha informações de texto na tela (posição, velocidade, controles).
 */
void drawHUD() {
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);

    // Salva e configura a matriz de projeção para 2D (ortogonal)
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
      glLoadIdentity();
      glOrtho(0,w,0,h,-1,1); // Projeção 2D de 0 a W e 0 a H.
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
        glLoadIdentity();
        glDisable(GL_LIGHTING); // Desabilita luz para que o texto não seja afetado.

        glColor3f(1,1,1);
        // Texto de instruções.
        std::string s = "Setas: dirigir carro   WASD/QE: mover camera   R: resetar   ESC: sair";
        glRasterPos2i(10, h-20);
        for(char c : s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);

        // Texto com o estado atual do carro (usa snprintf para formatar floats).
        char buf[128];
        snprintf(buf, sizeof(buf), "Carro: Pos (%.2f, %.2f) Direcao %.1f Velocidade %.2f Esterco %.1f",
                  car.x, car.z, car.heading, car.speed, car.wheelAngle);
        glRasterPos2i(10, h-36);
        for(char *p = buf; *p; ++p) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *p);

        glEnable(GL_LIGHTING); // Reabilita a iluminação para a renderização 3D.
      glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ----------------------- Display / Render ------------------------

/**
 * Função principal de desenho (callback de display).
 */
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Configura a matriz de visualização (câmera)
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // gluLookAt define a posição da câmera (cam.x, cam.y, cam.z), o ponto de foco (0.0f, 0.5f, 0.0f)
    // e o vetor 'up' (0.0f, 1.0f, 0.0f).
    gluLookAt(cam.x, cam.y, cam.z, 0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f);

    // Desenha a cena
    setupLighting();
    drawGroundTextured();
    for(auto &p : cones) drawConeAt(p.first, p.second);
    drawCarModel(car);

    // Desenha o HUD (em 2D por cima da cena 3D)
    drawHUD();

    glutSwapBuffers(); // Troca o buffer frontal e traseiro (para animação suave - double buffering).
}

// ----------------------- Reshape / Projection --------------------

/**
 * Função de callback chamada quando a janela é redimensionada.
 */
void reshape(int w, int h) {
    if(h==0) h=1;
    glViewport(0,0,w,h); // Ajusta o viewport para o novo tamanho da janela.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // Configura a perspectiva (FOV=60, AspectRatio, near, far).
    gluPerspective(60.0, (double)w/(double)h, 0.1, 300.0);
    glMatrixMode(GL_MODELVIEW);
}

// ----------------------- Timer loop ------------------------------

/**
 * Função de callback chamada periodicamente (a cada 16ms, ~60 FPS).
 */
void timerFunc(int value) {
    // Cálculo de Delta Time (dt) para movimentos independentes da taxa de quadros (FPS).
    int now = glutGet(GLUT_ELAPSED_TIME);
    static int last = now;
    float dt = (now - last) / 1000.0f; // dt em segundos
    if(dt <= 0.0f || dt > 0.5f) dt = 0.016f; // Evita valores extremos.
    last = now;

    // Atualiza o estado da simulação.
    updatePhysics(dt);

    glutPostRedisplay(); // Marca a janela para ser redesenhada no próximo loop.
    glutTimerFunc(16, timerFunc, 0); // Reagenda o timer para 16ms (60 FPS).
}

// ----------------------- Main ------------------------------------

int main(int argc, char** argv) {
    // Inicialização do GLUT/FreeGLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH); // Double buffer, cor RGB, buffer de profundidade.
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("Baliza - Asfalto Procedural, Cones, Carro e Camera");

    // Configurações iniciais de estado do OpenGL
    glEnable(GL_DEPTH_TEST);  // Habilita o teste de profundidade para objetos 3D.
    glShadeModel(GL_SMOOTH);  // Interpola cores e normais.
    glEnable(GL_NORMALIZE);   // Garante que normais sejam unitárias após transformações.

    // Inicialização da cena
    initTextures();
    setupConesStraightCorridor();
    setupLighting();

    // Configura os callbacks (funções que serão chamadas em eventos)
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);  // Teclas regulares pressionadas
    glutKeyboardUpFunc(keyboardUp);  // Teclas regulares liberadas
    glutSpecialFunc(specialDown);    // Teclas especiais (setas) pressionadas
    glutSpecialUpFunc(specialUp);    // Teclas especiais (setas) liberadas

    // Inicia o loop do timer (animação)
    glutTimerFunc(16, timerFunc, 0);

    // Inicia o loop principal do FreeGLUT (mantém a janela aberta e processa eventos)
    glutMainLoop();
    return 0;
}