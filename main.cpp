#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --- VARIABLES GLOBALES ---
GLuint texturaTerreno;
GLuint texturaAsfalto;
bool modoMirador = false;
// Cámara (Orbital + Desplazamiento)
float camYaw = 45.0f;
float camPitch = 45.0f;
float radioZoom = 250.0f;
float targetX = 0.0f;     // Punto central al que mira la cámara
float targetZ = 0.0f;     // Nos permite movernos con WASD

int lastMouseX = -1;
int lastMouseY = -1;
bool isDragging = false;

// --- FUNCIÓN DE CARGA DE TEXTURA (Optimizada) ---
GLuint cargarTextura(const char* ruta) {
    GLuint texturaID;
    glGenTextures(1, &texturaID);
    glBindTexture(GL_TEXTURE_2D, texturaID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Filtros lineales para suavizar el mapeo
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    unsigned char *data = stbi_load(ruta, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum formato = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, formato, width, height, 0, formato, GL_UNSIGNED_BYTE, data);
        printf("EXITO: Textura cargada -> %s\n", ruta);
    } else {
        printf("ERROR CRITICO: No se pudo encontrar -> %s\n", ruta);
    }
    stbi_image_free(data);
    return texturaID;
}

// --- GENERACIÓN DE RELIEVE PROCEDURAL ---
// Usamos fractales basados en senos/cosenos para simular dunas y erosión rocosa
// --- GENERACIÓN DE RELIEVE PROCEDURAL (Más plano) ---
float getHeight(float x, float z) {
    float h = 0.0f;

    // 1. Montañas y cerros grandes (le dan volumen al horizonte)
    h += sin(x * 0.015f) * cos(z * 0.015f) * 25.0f;

    // 2. Dunas o elevaciones medianas
    h += sin(x * 0.04f + z * 0.03f) * 3.0f;

    // 3. Detalles de rocas en el suelo
    h += sin(x * 0.3f) * cos(z * 0.3f) * 0.5f;

    // Suavizar el centro (Pampas) pero sin dejarlo plano como una mesa
    float distCentro = sqrt(x*x + z*z);
    if (distCentro < 180.0f) {
        // En lugar de aplanar a 0, reducimos la altura a un 10% en el centro exacto
        // y va subiendo hasta la altura normal a medida que te alejas
        float factorSuavizado = 0.1f + 0.9f * (distCentro / 180.0f);
        h *= factorSuavizado;
    }

    return h;
}

// --- DIBUJAR TERRENO CON NORMALES PARA LUZ ---
void drawTerrain() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texturaTerreno);

    glColor3f(0.65f, 0.70f, 0.75f);

    GLfloat mat_ambient[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat mat_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);

    // --- NUEVOS LÍMITES RECTANGULARES ---
    float limiteX = 500.0f; // Más ancho (Izquierda - Derecha)
    float limiteZ = 320.0f; // Menos profundo (Adelante - Atrás)
    float resolucion = 5.0f;

    for (float z = -limiteZ; z < limiteZ; z += resolucion) {
        glBegin(GL_TRIANGLE_STRIP);
        for (float x = -limiteX; x <= limiteX; x += resolucion) {
            for (int i = 0; i <= 1; i++) {
                float actualZ = z + i * resolucion;
                float y = getHeight(x, actualZ);

                float hL = getHeight(x - 1.0f, actualZ);
                float hR = getHeight(x + 1.0f, actualZ);
                float hD = getHeight(x, actualZ - 1.0f);
                float hU = getHeight(x, actualZ + 1.0f);
                float nx = hL - hR;
                float ny = 2.0f;
                float nz = hD - hU;
                float len = sqrt(nx*nx + ny*ny + nz*nz);

                glNormal3f(nx/len, ny/len, nz/len);

                glTexCoord2f(x / 500.0f, actualZ / 500.0f);
                glVertex3f(x, y, actualZ);
            }
        }
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
}

// --- RESERVA DE ESPACIOS (Marcadores) ---
// --- DIBUJAR TEXTO FLOTANTE (Ahora sí, 100% visible y blanco) ---
void dibujarTextoFlotante(float x, float z, const std::string& texto) {
    float y_terreno = getHeight(x, z);

    // --- APAGAMOS TODO LO QUE INTERFIERE ---
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D); // <--- ESTO EVITA QUE LA LETRA SE VEA DEL COLOR DE LA TIERRA

    // 1. Círculo Amarillo
    glColor3f(1.0f, 1.0f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    float radio = 15.0f;
    for(int i = 0; i < 360; i += 10) {
        float rad = i * 3.14159f / 180.0f;
        glVertex3f(x + cos(rad)*radio, y_terreno + 0.5f, z + sin(rad)*radio);
    }
    glEnd();

    // 2. Texto Blanco Puro
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos3f(x, y_terreno + 25.0f, z);
    for (char c : texto) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    // --- REACTIVAMOS TODO ---
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}
// --- CÁLCULO DE LA CURVA DE LA CARRETERA (Ajustada a la escala) ---
float calcularPosXCarretera(float z) {
    // Diagonal base
    float xBase = 1.3f * z + 30.0f;

    // Curva del Mirador (Campana de Gauss MUCHO más pronunciada)
    // -90.0f es la "profundidad" de la curva hacia la izquierda.
    // 3000.0f controla qué tan larga/extendida es la curva.
    float curva = exp(-(z * z) / 3000.0f) * -90.0f;

    return xBase + curva;
}
// --- DIBUJAR CARRETERA PANAMERICANA (Con la Curva del Mirador) ---
void drawCarretera() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texturaAsfalto);

    glColor3f(1.0f, 1.0f, 1.0f);

    GLfloat mat_ambient[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat mat_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);

    float ancho = 4.0f;
    float limiteZ = 320.0f;

    glBegin(GL_TRIANGLE_STRIP);
    for (float z = -limiteZ; z <= limiteZ; z += 5.0f) {
        // Obtenemos la posición X con su respectiva curva
        float posX = calcularPosXCarretera(z);

        float yIzquierda = getHeight(posX - ancho, z) + 0.3f;
        float yDerecha = getHeight(posX + ancho, z) + 0.3f;

        glNormal3f(0.0f, 1.0f, 0.0f);

        glTexCoord2f(0.0f, z / 10.0f);
        glVertex3f(posX - ancho, yIzquierda, z);

        glTexCoord2f(1.0f, z / 10.0f);
        glVertex3f(posX + ancho, yDerecha, z);
    }
    glEnd();

    // --- DIBUJAR LA LÍNEA AMARILLA CENTRAL ---
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);

    glColor3f(0.8f, 0.7f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (float z = -limiteZ; z <= limiteZ; z += 15.0f) {
        // Punto inicial del segmento punteado
        float x1 = calcularPosXCarretera(z);
        float y1 = getHeight(x1, z) + 0.4f;

        // Punto final del segmento punteado
        float z2 = z + 7.5f;
        float x2 = calcularPosXCarretera(z2);
        float y2 = getHeight(x2, z2) + 0.4f;

        glVertex3f(x1, y1, z);
        glVertex3f(x2, y2, z2);
    }
    glEnd();

    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
}
// --- DIBUJAR MARCADORES (Ajustados a la carretera diagonal) ---
// --- DIBUJAR MARCADORES (Despejando la carretera) ---
void drawMarkers() {
    // === GRUPO IZQUIERDA (Arriba de la Carretera) ===
    dibujarTextoFlotante(-120.0f, -260.0f, "ESTRELLA");
    dibujarTextoFlotante(60.0f, -270.0f, "COLIBRI DE PALPA");

    dibujarTextoFlotante(-350.0f, -100.0f, "7. COLIBRI");
    dibujarTextoFlotante(-380.0f, 30.0f, "5. MONO");

    // La Araña y el Cóndor bien alejados de la pista hacia la izquierda
    dibujarTextoFlotante(-180.0f, -50.0f, "8. ARANA");
    dibujarTextoFlotante(-200.0f, 50.0f, "9. CONDOR");

    dibujarTextoFlotante(-300.0f, 160.0f, "6. PERRO");
    dibujarTextoFlotante(-200.0f, 140.0f, "2. COMPAS");
    dibujarTextoFlotante(-100.0f, 220.0f, "3. TRAPEZOIDES");

    // === GRUPO DERECHA (Abajo de la Carretera) ===

    // Figuras del Mirador (Empujadas a la izquierda, dentro de la curva)
    dibujarTextoFlotante(-140.0f, -15.0f, "13. ARBOL");
    dibujarTextoFlotante(-40.0f, 40.0f, "12. MANOS");

    // El Astronauta a la izquierda de la pista recta
    dibujarTextoFlotante(100.0f, 140.0f, "4. ASTRONAUTA");

    // Otras figuras de la derecha
    dibujarTextoFlotante(280.0f, -120.0f, "10. ALCATRAZ");
    dibujarTextoFlotante(380.0f, 20.0f, "11. LORO");

    // La Ballena en la zona inferior izquierda de la carretera
    dibujarTextoFlotante(220.0f, 240.0f, "1. BALLENA");
}
// --- CONTROLES Y CÁMARA ---
void teclado(unsigned char key, int x, int y) {
    // Calculamos los vectores de dirección basados en hacia dónde miramos
    float yawRad = camYaw * 3.14159f / 180.0f;
    float forwardX = -sin(yawRad);
    float forwardZ = -cos(yawRad);
    float rightX = cos(yawRad);
    float rightZ = -sin(yawRad);
    float velocidad = 5.0f;

    switch(tolower(key)) {
        // Movimiento WASD
        case 'w': targetX += forwardX * velocidad; targetZ += forwardZ * velocidad; break;
        case 's': targetX -= forwardX * velocidad; targetZ -= forwardZ * velocidad; break;
        case 'a': targetX -= rightX * velocidad; targetZ -= rightZ * velocidad; break;
        case 'd': targetX += rightX * velocidad; targetZ += rightZ * velocidad; break;

        // Modos de cámara
        case '1': // MODO DRON
            radioZoom = 200.0f; camPitch = 45.0f;
            break;
        case '2': // MODO EXPLORADOR (Suelo)
            radioZoom = 40.0f; camPitch = 5.0f;
            break;
        case '3': // MODO SATÉLITE
            radioZoom = 400.0f; camPitch = 85.0f;
            break;
    }
    glutPostRedisplay();
}

void raton(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        isDragging = (state == GLUT_DOWN);
        if (isDragging) { lastMouseX = x; lastMouseY = y; }
    }
    if (button == 3) radioZoom -= 15.0f; // Zoom in
    if (button == 4) radioZoom += 15.0f; // Zoom out
    if (radioZoom < 10.0f) radioZoom = 10.0f;
    glutPostRedisplay();
}

void movimientoRaton(int x, int y) {
    if (isDragging) {
        camYaw += (x - lastMouseX) * 0.4f;
        camPitch += (y - lastMouseY) * 0.4f;
        if (camPitch > 85.0f) camPitch = 85.0f;
        if (camPitch < 1.0f) camPitch = 1.0f;
        lastMouseX = x; lastMouseY = y;
        glutPostRedisplay();
    }
}

// --- RENDERIZADO PRINCIPAL ---
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Cámara interactiva
    float yawRad = camYaw * 3.14159f / 180.0f;
    float pitchRad = camPitch * 3.14159f / 180.0f;
    float camX = targetX + radioZoom * cos(pitchRad) * sin(yawRad);
    float camY = getHeight(targetX, targetZ) + radioZoom * sin(pitchRad); // Adaptable a la altura
    float camZ = targetZ + radioZoom * cos(pitchRad) * cos(yawRad);

    gluLookAt(camX, camY, camZ, targetX, getHeight(targetX, targetZ), targetZ, 0.0f, 1.0f, 0.0f);

// Posición de la luz del atardecer / sol de mediodía...
    GLfloat light_pos[] = { 200.0f, 100.0f, 200.0f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

    drawTerrain();
    drawCarretera(); // <---- AÑADE ESTA LÍNEA AQUÍ
    drawMarkers();

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (float)w/h, 1.0f, 2000.0f); // Perspectiva amplia
    glMatrixMode(GL_MODELVIEW);
}

void init() {
    glEnable(GL_DEPTH_TEST);

    // Cielo grisáceo/azulado claro, cero tonos cálidos
    GLfloat skyColor[] = { 0.65f, 0.70f, 0.75f, 1.0f };
    glClearColor(skyColor[0], skyColor[1], skyColor[2], skyColor[3]);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // LUZ TOTALMENTE BLANCA Y NEUTRA
    GLfloat light_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f }; // Ilumina sombras sin dar color
    GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f }; // Luz del sol blanca pura
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);

    glEnable(GL_FOG);
    glFogfv(GL_FOG_COLOR, skyColor);
    glFogi(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, 0.001f); // Visibilidad lejana
    glHint(GL_FOG_HINT, GL_NICEST);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

texturaTerreno = cargarTextura("C:/Users/JARDY/Desktop/Proyecto_Nazca/dirt_aerial_02_diff_4k.jpg");
    texturaAsfalto = cargarTextura("C:/Users/JARDY/Desktop/Proyecto_Nazca/asfalto.png");
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768); // Resolución panorámica
    glutCreateWindow("Proyecto_Nazca - Entorno");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(teclado);
    glutMouseFunc(raton);
    glutMotionFunc(movimientoRaton);

    glutMainLoop();
    return 0;
}
