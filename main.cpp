#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
GLuint idTexturaSurco; // <-- Ponlo aquí, completamente afuera de las funciones
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
float targetX = 0.0f;
float targetZ = 0.0f;

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

// --- GENERACIÓN DE RELIEVE PROCEDURAL (Más plano) ---
float getHeight(float x, float z) {
    float h = 0.0f;
    h += sin(x * 0.015f) * cos(z * 0.015f) * 25.0f;
    h += sin(x * 0.04f + z * 0.03f) * 3.0f;
    h += sin(x * 0.3f) * cos(z * 0.3f) * 0.5f;

    float distCentro = sqrt(x*x + z*z);
    if (distCentro < 180.0f) {
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

    float limiteX = 500.0f;
    float limiteZ = 320.0f;
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

// --- VECTORES DE LOS TRAZOS ---
struct Punto2D {
    float x, z;
};

// Trazo Ballena (Cuerpo)
std::vector<Punto2D> trazo1 = {
    {5.70752f, 12.9926f}, {7.12257f, 7.90292f}, {12.667f, 7.17681f}, {13.702f, 9.29824f}, {18.8161f, 14.1183f},
    {19.9464f, 13.0642f}, {19.239f, 9.38598f}, {18.2006f, 4.94299f}, {18.8034f, 3.14975f}, {24.2752f, 1.79679f},
    {25.6288f, 0.878347f}, {25.3528f, -0.741634f}, {18.0498f, -0.43454f}, {18.4495f, -3.50735f}, {25.0179f, -3.65427f},
    {24.9985f, -5.91439f}, {22.7359f, -7.15231f}, {9.04496f, -7.45503f}, {4.69572f, -3.56704f}, {-10.174f, -1.60381f},
    {-9.12432f, -15.467f}, {-15.0327f, -4.85044f}, {-14.6992f, -1.90567f}, {-28.7213f, 1.61889f}, {-28.5312f, 4.67726f},
    {-13.761f, 0.832781f}, {-4.76467f, 4.59891f}, {-0.857974f, 5.86794f}, {2.91187f, 6.52112f}, {-0.591784f, 17.2018f},
    {0.414608f, 18.7387f}, {2.2034f, 17.7432f}, {5.55604f, 13.0691f}
};

// Trazo Compás
std::vector<Punto2D> trazo2 = {
    {25.8653f, -2.37546f}, {24.1587f, -3.03077f}, {17.7535f, -0.840849f}, {-3.92697f, 7.41647f}, {15.2688f, -3.83637f},
    {-3.53896f, -4.43292f}, {14.2763f, -8.11192f}, {-6.14224f, -12.238f}, {21.7849f, -10.5906f}, {23.7946f, -12.095f},
    {25.7413f, -12.9605f}, {27.5763f, -13.1965f}, {29.6502f, -13.3737f}, {31.249f, -12.8026f}, {33.2604f, -11.0258f},
    {39.7918f, -12.9492f}, {41.0086f, -8.58303f}, {34.0411f, -9.02233f}, {33.944f, -6.68402f}, {33.5979f, -5.61469f},
    {32.9353f, -4.68144f}, {32.2849f, -3.81393f}, {31.0236f, -3.15148f}, {29.7194f, -2.74537f}, {28.5527f, -2.32277f},
    {27.2457f, -1.96076f}, {25.7822f, -2.32336f}
};

// Trazo Perro
std::vector<Punto2D> trazo3 = {
    {1.38896f, -2.63996f}, {0.290128f, -1.4594f}, {0.493796f, -0.351969f}, {1.68721f, -0.324901f}, {2.52971f, -1.07234f},
    {3.40383f, -2.4922f}, {4.91286f, -2.4527f}, {5.92515f, -1.74686f}, {6.22347f, -0.819319f}, {5.66208f, 0.124973f},
    {3.37485f, 1.61394f}, {2.23186f, 2.61764f}, {2.23953f, 3.63836f}, {3.09345f, 4.93577f}, {3.03197f, 5.56066f},
    {2.13861f, 5.44212f}, {0.245509f, 2.74905f}, {-0.553396f, 2.16043f}, {-1.04592f, 2.55853f}, {0.28033f, 4.55562f},
    {0.148258f, 5.08221f}, {-1.13708f, 4.60456f}, {-2.53357f, 2.47853f}, {-3.05897f, 2.12553f}, {-3.92963f, 2.08008f},
    {-9.23881f, 2.45116f}, {-15.6438f, 6.64851f}, {-21.4657f, 8.98187f}, {-24.2115f, 9.47861f}, {-28.465f, 7.80772f},
    {-31.5598f, 7.14917f}, {-30.6232f, 8.16489f}, {-19.6539f, 16.7374f}, {-16.606f, 19.7626f}, {-10.1254f, 26.0249f},
    {-9.38266f, 27.1594f}, {-10.4717f, 27.4808f}, {-21.7761f, 17.1553f}, {-32.9095f, 8.65204f}, {-35.4612f, 6.1703f},
    {-35.6874f, 5.13801f}, {-33.2933f, 4.2792f}, {-33.6498f, 3.78091f}, {-34.5962f, 2.97226f}, {-41.1402f, 0.348285f},
    {-50.4665f, -3.64102f}, {-57.0217f, -6.36707f}, {-64.7696f, -9.66688f}, {-65.2001f, -11.181f}, {-33.4989f, 1.67401f},
    {-29.3341f, 3.29792f}, {-29.0249f, 1.87143f}, {-41.9745f, -14.7755f}, {-44.9212f, -15.0012f}, {-45.0122f, -15.6484f},
    {-44.1528f, -16.1401f}, {-45.1272f, -17.0388f}, {-51.2117f, -18.8184f}, {-51.7421f, -19.5588f}, {-51.0865f, -19.9765f},
    {-46.4537f, -18.0758f}, {-45.9781f, -18.4376f}, {-51.5319f, -21.369f}, {-50.9664f, -21.7588f}, {-45.4238f, -19.1535f},
    {-44.8924f, -19.3352f}, {-49.5241f, -22.1582f}, {-49.4584f, -22.8862f}, {-41.6648f, -18.1174f}, {-29.5729f, -2.04873f},
    {-26.9112f, 1.45115f}, {-25.7216f, 1.16374f}, {-26.3856f, -0.324841f}, {-38.9327f, -16.8779f}, {-44.4618f, -21.0015f},
    {-45.5324f, -21.5741f}, {-44.7309f, -21.8398f}, {-41.5664f, -19.5818f}, {-41.1188f, -20.0336f}, {-42.1927f, -21.8885f},
    {-41.0931f, -22.1479f}, {-39.5771f, -19.4768f}, {-38.7661f, -18.7583f}, {-38.0764f, -19.384f}, {-39.3521f, -21.571f},
    {-38.4325f, -21.7746f}, {-37.46f, -20.2178f}, {-36.568f, -17.6491f}, {-23.9109f, -0.0779185f}, {-22.7739f, 0.736781f},
    {-21.5564f, 0.539399f}, {-22.9438f, -2.00771f}, {-21.8784f, -2.05916f}, {-18.4501f, 0.41301f}, {-18.0522f, -0.516916f},
    {-31.2053f, -20.9287f}, {-35.0794f, -22.9012f}, {-35.5757f, -23.8496f}, {-32.1847f, -22.4516f}, {-35.078f, -25.7156f},
    {-35.4501f, -26.9f}, {-33.6611f, -25.8929f}, {-32.6606f, -24.2462f}, {-31.9212f, -24.3938f}, {-32.2653f, -25.4882f},
    {-32.9137f, -26.2907f}, {-31.5284f, -25.9479f}, {-30.7298f, -24.6333f}, {-29.7695f, -22.4805f}, {-16.0066f, -0.712256f},
    {-14.5934f, -0.279451f}, {-13.7918f, -0.545071f}, {-26.7961f, -22.385f}, {-28.4565f, -23.2681f}, {-28.5429f, -23.8502f},
    {-28.1295f, -24.094f}, {-29.5075f, -25.657f}, {-29.7739f, -26.1833f}, {-29.0965f, -26.2129f}, {-28.1117f, -25.2523f},
    {-27.195f, -23.8756f}, {-26.7195f, -24.2375f}, {-27.6815f, -25.7322f}, {-27.4078f, -26.4262f}, {-25.1934f, -24.3667f},
    {-20.8775f, -24.4404f}, {-20.1006f, -23.5139f}, {-20.5097f, -22.91f}, {-24.0948f, -22.9981f}, {-24.0723f, -21.6399f},
    {-11.0049f, -0.840141f}, {-10.3337f, -0.571767f}, {-9.08818f, -0.679121f}, {-4.04594f, -1.27444f}, {-3.39034f, -1.69216f},
    {-2.28461f, -2.24965f}, {-1.79673f, -3.20753f}, {-0.144677f, -4.23077f}, {1.0667f, -4.13012f}, {1.41523f, -2.67578f}
};

// Trazo Trapezoide (Lineas)
std::vector<Punto2D> trazo4 = {
    {144.976f, -59.5811f}, {-55.0388f, -28.6923f}, {144.99f, -7.1143f}, {144.976f, -59.5811f}
};

// Trazo Ballena (Ojo/Espiral)
std::vector<Punto2D> trazo5 = {
    {13.6752f, -1.92595f}, {13.6662f, -2.18673f}, {13.737f, -2.41509f}, {13.8759f, -2.58721f}, {14.14f, -2.69238f},
    {14.4079f, -2.60121f}, {14.5808f, -2.32049f}, {14.5859f, -1.95883f}, {14.3851f, -1.65957f}, {14.0925f, -1.44864f},
    {13.5741f, -1.24864f}, {13.0353f, -1.44815f}, {12.6419f, -1.96071f}, {12.5991f, -2.6318f}, {12.9494f, -3.24726f},
    {13.8438f, -3.46979f}, {14.551f, -3.34378f}, {14.9462f, -2.75833f}, {15.0419f, -2.01856f}, {14.554f, -1.09886f},
    {13.8089f, -0.586208f}, {12.7701f, -0.501034f}, {11.5462f, -0.535154f}, {10.0963f, -0.69168f}, {8.14749f, -0.94619f},
    {6.4614f, -1.16586f}
};

// Trazo Trapezoide (Escaleras) - Original
std::vector<Punto2D> trazo6 = {
    {15.9019f, 92.1882f}, {-58.2729f, 62.4144f}, {20.6884f, 63.2381f}, {19.1379f, 73.3706f}, {31.7026f, 72.2616f},
    {30.4248f, 85.3464f}, {18.4645f, 78.7347f}, {15.4261f, 91.9486f}, {19.5692f, 70.0343f}
};

// Función para rotar trazos respetando las coordenadas reales para el getHeight()
std::vector<Punto2D> rotarTrazo(const std::vector<Punto2D>& trazoOriginal, float anguloGrados) {
    std::vector<Punto2D> trazoRotado;
    float anguloRadianes = anguloGrados * (3.14159265f / 180.0f);
    float cosA = std::cos(anguloRadianes);
    float sinA = std::sin(anguloRadianes);

    for (const auto& punto : trazoOriginal) {
        Punto2D nuevoPunto;
        nuevoPunto.x = punto.x * cosA - punto.z * sinA;
        nuevoPunto.z = punto.x * sinA + punto.z * cosA;
        trazoRotado.push_back(nuevoPunto);
    }
    return trazoRotado;
}
// --- FUNCIÓN PARA DIBUJAR LOS TRAZOS DE LAS FIGURAS EN SU SITIO ---
void drawFigures() {
    // 1. ACTIVAMOS EL MOTOR DE TEXTURAS DESDE EL INICIO
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG); // Apagamos la niebla para que no pinte de gris las figuras

    // Usamos GL_REPLACE para que pinte exactamente los píxeles marrón-ocre de lineas.jpg
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glLineWidth(3.0f);

    // Activamos Polygon Offset fuerte para evitar el parpadeo con el suelo
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-4.0f, -4.0f);

    // --- DEFINICIÓN DE LA LAMBDA ---
    auto dibujarTrazo = [](const std::vector<Punto2D>& trazo, float offsetX, float offsetZ, float escala = 1.0f) {
        if (trazo.empty()) return;

        float resolucion = 1.0f;
        float anchoLinea = 0.8f; // Grosor ideal para que se note la textura de tierra

        glBindTexture(GL_TEXTURE_2D, idTexturaSurco);

        glBegin(GL_TRIANGLE_STRIP);
        float distanciaAcumulada = 0.0f; // Nos servirá para avanzar en la textura a lo largo de la línea

        for (size_t i = 0; i < trazo.size() - 1; ++i) {
            float x1 = (trazo[i].x * escala) + offsetX;
            float z1 = (trazo[i].z * escala) + offsetZ;
            float x2 = (trazo[i+1].x * escala) + offsetX;
            float z2 = (trazo[i+1].z * escala) + offsetZ;

            float dx = x2 - x1;
            float dz = z2 - z1;
            float distancia = sqrt(dx*dx + dz*dz);

            if (distancia < 0.01f) continue;

            int pasos = std::max(1, (int)(distancia / resolucion));

            float nx = -dz / distancia;
            float nz = dx / distancia;

            for (int j = 0; j <= pasos; ++j) {
                float t = (float)j / (float)pasos;
                float curX = x1 + dx * t;
                float curZ = z1 + dz * t;

                float alturaBase = getHeight(curX, curZ);
                // Levantamos sutilmente (+0.2f) para que jamás se mezcle con el terreno base
                float curY = alturaBase + 0.2f;

                // === EL CAMBIO CLAVE EN LAS COORDENADAS DE TEXTURA ===
                // En vez de clavar 0.0 y 1.0, mapeamos usando las posiciones reales en el mundo (curX, curZ).
                // Al multiplicar por 0.1f logramos que la textura "abrace" el relieve de la montaña de Nazca de forma fluida.
                float u = curX * 0.1f;
                float v = curZ * 0.1f;

                // Vértice Izquierdo de la cinta
                glTexCoord2f(u, v);
                glVertex3f(curX - nx * anchoLinea, curY, curZ - nz * anchoLinea);

                // Vértice Derecho de la cinta
                glTexCoord2f(u + 0.05f, v + 0.05f); // Un leve offset para dar volumen transicional
                glVertex3f(curX + nx * anchoLinea, curY, curZ + nz * anchoLinea);
            }
        }
        glEnd();
    };

    // --- PINTAR LAS FIGURAS (En blanco para conservar el color de la textura) ---
    glColor3f(1.0f, 1.0f, 1.0f);

    // Ballena
    dibujarTrazo(rotarTrazo(trazo1, 210.0f), 220.0f, 250.0f, 3.5f);
    dibujarTrazo(rotarTrazo(trazo5, 210.0f), 220.0f, 250.0f, 3.5f);

    // Resto de figuras en sus posiciones respectivas
    dibujarTrazo(trazo2, -200.0f, 140.0f, 4.0f);   // Compás
    dibujarTrazo(trazo3, -450.0f, 180.0f, -3.0f);  // Perro
    dibujarTrazo(trazo4, -100.0f, 240.0f, 1.0f);   // Trapezoide grande
    dibujarTrazo(trazo6, -30.0f, 85.0f, 2.5f);     // Trapezoide chico

    // --- RESTAURAR ESTADOS GLOBALES DE OPENGL ---
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_TEXTURE_2D);

    // Regresamos el modo de textura por defecto para que la carretera y el mapa no fallen
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}
// --- DIBUJAR TEXTO FLOTANTE ---
void dibujarTextoFlotante(float x, float z, const std::string& texto) {
    float y_terreno = getHeight(x, z);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

    glColor3f(1.0f, 1.0f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    float radio = 15.0f;
    for(int i = 0; i < 360; i += 10) {
        float rad = i * 3.14159f / 180.0f;
        glVertex3f(x + cos(rad)*radio, y_terreno + 0.5f, z + sin(rad)*radio);
    }
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos3f(x, y_terreno + 25.0f, z);
    for (char c : texto) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

// --- CÁLCULO Y DIBUJO DE LA CARRETERA ---
float calcularPosXCarretera(float z) {
    float xBase = 1.3f * z + 30.0f;
    float curva = exp(-(z * z) / 3000.0f) * -90.0f;
    return xBase + curva;
}

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
        float posX = calcularPosXCarretera(z);
        float yIzquierda = getHeight(posX - ancho, z) + 0.3f;
        float yDerecha = getHeight(posX + ancho, z) + 0.3f;

        glNormal3f(0.0f, 1.0f, 0.0f);
        glTexCoord2f(0.0f, z / 10.0f); glVertex3f(posX - ancho, yIzquierda, z);
        glTexCoord2f(1.0f, z / 10.0f); glVertex3f(posX + ancho, yDerecha, z);
    }
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glColor3f(0.8f, 0.7f, 0.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (float z = -limiteZ; z <= limiteZ; z += 15.0f) {
        float x1 = calcularPosXCarretera(z);
        float y1 = getHeight(x1, z) + 0.4f;
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

// --- DIBUJAR MARCADORES ---
void drawMarkers() {

    // Otros marcadores originales que tenías
    dibujarTextoFlotante(-120.0f, -260.0f, "ESTRELLA");
    dibujarTextoFlotante(60.0f, -270.0f, "COLIBRI DE PALPA");
    dibujarTextoFlotante(-350.0f, -100.0f, "7. COLIBRI");
    dibujarTextoFlotante(-380.0f, 30.0f, "5. MONO");
    dibujarTextoFlotante(-180.0f, -50.0f, "8. ARANA");
    dibujarTextoFlotante(-200.0f, 50.0f, "9. CONDOR");
    dibujarTextoFlotante(-140.0f, -15.0f, "13. ARBOL");
    dibujarTextoFlotante(-40.0f, 40.0f, "12. MANOS");
    dibujarTextoFlotante(100.0f, 140.0f, "4. ASTRONAUTA");
    dibujarTextoFlotante(280.0f, -120.0f, "10. ALCATRAZ");
    dibujarTextoFlotante(380.0f, 20.0f, "11. LORO");
}
//INSTRUCCIONES INICIALES:
void mostrarInstrucciones() {
    std::cout << "===================================================================\n";
    std::cout << "             BIENVENIDO A LA SIMULACION DE NAZCA 3D                \n";
    std::cout << "===================================================================\n";
    std::cout << " INSTRUCCIONES DE NAVEGACION PARA EL USUARIO:\n\n";
    std::cout << " 1. DESPLAZAMIENTO (Caminar por el mapa):\n";
    std::cout << "    [W] -> Avanzar hacia adelante\n";
    std::cout << "    [S] -> Retroceder hacia atras\n";
    std::cout << "    [A] -> Desplazarse a la izquierda\n";
    std::cout << "    [D] -> Desplazarse a la derecha\n\n";
    std::cout << " 2. CONTROL DE LA CAMARA (Mirar alrededor):\n";
    std::cout << "    * Manten presionado el CLICK IZQUIERDO del raton y arrastralo\n";
    std::cout << "      para girar la cabeza y orientar la vista.\n\n";
    std::cout << " 3. ZOOM (Altura de vuelo):\n";
    std::cout << "    * Usa la RUEDA DEL RATON (Scroll) para acercarte o alejarte.\n\n";
    std::cout << " 4. CAMARAS PREDEFINIDAS (Vistas rapidas):\n";
    std::cout << "    [1] -> Vista Intermedia (En diagonal)\n";
    std::cout << "    [2] -> Vista Terrestre (Desde el suelo)\n";
    std::cout << "    [3] -> Vista Satelital (Desde un avion)\n\n";
    std::cout << " -----------------------------------------------------------------\n";
    std::cout << " NOTA CLAVE: Haz un clic sobre la ventana grafica donde aparece\n";
    std::cout << " el mapa para asegurarte de que capture tus teclas y movimientos.\n";
    std::cout << "===================================================================\n\n";
}
// --- CONTROLES Y CÁMARA ---
void teclado(unsigned char key, int x, int y) {
    float yawRad = camYaw * 3.14159f / 180.0f;
    float forwardX = -sin(yawRad); float forwardZ = -cos(yawRad);
    float rightX = cos(yawRad); float rightZ = -sin(yawRad);
    float velocidad = 5.0f;

    switch(tolower(key)) {
        case 'w': targetX += forwardX * velocidad; targetZ += forwardZ * velocidad; break;
        case 's': targetX -= forwardX * velocidad; targetZ -= forwardZ * velocidad; break;
        case 'a': targetX -= rightX * velocidad; targetZ -= rightZ * velocidad; break;
        case 'd': targetX += rightX * velocidad; targetZ += rightZ * velocidad; break;
        case '1': radioZoom = 200.0f; camPitch = 45.0f; break;
        case '2': radioZoom = 40.0f; camPitch = 5.0f; break;
        case '3': radioZoom = 400.0f; camPitch = 85.0f; break;
    }
    glutPostRedisplay();
}

void raton(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        isDragging = (state == GLUT_DOWN);
        if (isDragging) { lastMouseX = x; lastMouseY = y; }
    }
    if (button == 3) radioZoom -= 15.0f;
    if (button == 4) radioZoom += 15.0f;
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

    float yawRad = camYaw * 3.14159f / 180.0f;
    float pitchRad = camPitch * 3.14159f / 180.0f;
    float camX = targetX + radioZoom * cos(pitchRad) * sin(yawRad);
    float camY = getHeight(targetX, targetZ) + radioZoom * sin(pitchRad);
    float camZ = targetZ + radioZoom * cos(pitchRad) * cos(yawRad);

    gluLookAt(camX, camY, camZ, targetX, getHeight(targetX, targetZ), targetZ, 0.0f, 1.0f, 0.0f);

    GLfloat light_pos[] = { 200.0f, 100.0f, 200.0f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

    drawTerrain();
    drawCarretera();
    drawFigures();
    drawMarkers();

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (float)w/h, 1.0f, 2000.0f);
    glMatrixMode(GL_MODELVIEW);
}

void init() {
    glEnable(GL_DEPTH_TEST);

    GLfloat skyColor[] = { 0.65f, 0.70f, 0.75f, 1.0f };
    glClearColor(skyColor[0], skyColor[1], skyColor[2], skyColor[3]);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat light_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);

    glEnable(GL_FOG);
    glFogfv(GL_FOG_COLOR, skyColor);
    glFogi(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, 0.001f);
    glHint(GL_FOG_HINT, GL_NICEST);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    texturaTerreno = cargarTextura("C:/Users/JARDY/Desktop/Proyecto_Nazca/dirt_aerial_02_diff_4k.jpg");
    texturaAsfalto = cargarTextura("C:/Users/JARDY/Desktop/Proyecto_Nazca/asfalto.png");
    idTexturaSurco = cargarTextura("C:/Users/JARDY/Desktop/Proyecto_Nazca/tierra.jpg"); // Pon la ruta correcta a tu imagen de tierra
}

int main(int argc, char** argv) {
    mostrarInstrucciones();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768);
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
