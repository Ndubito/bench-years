#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <regex>
#include <cstdlib>

struct Pt2 { float x, y; };
struct Shape { std::string kind; float z = 0; unsigned int rgba = 0xFFFFFFFFu; float r = 0; std::vector<Pt2> pts; };

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static float getFloat(const std::string& obj, const std::string& key) {
    size_t p = obj.find("\"" + key + "\"");
    if (p == std::string::npos) return 0.f;
    p = obj.find(':', p); if (p == std::string::npos) return 0.f;
    size_t start = obj.find_first_of("-0123456789.", p);
    if (start == std::string::npos) return 0.f;
    size_t end = obj.find_first_not_of("0123456789.eE+-", start);
    try { return std::stof(obj.substr(start, end - start)); } catch (...) { return 0.f; }
}

static unsigned int getUint(const std::string& obj, const std::string& key) {
    size_t p = obj.find("\"" + key + "\"");
    if (p == std::string::npos) return 0u;
    p = obj.find(':', p); if (p == std::string::npos) return 0u;
    size_t start = obj.find_first_of("0123456789", p);
    if (start == std::string::npos) return 0u;
    size_t end = obj.find_first_not_of("0123456789", start);
    try { return (unsigned int)std::stoul(obj.substr(start, end - start)); } catch (...) { return 0u; }
}

static std::string getStr(const std::string& obj, const std::string& key) {
    size_t p = obj.find("\"" + key + "\"");
    if (p == std::string::npos) return {};
    p = obj.find(':', p); if (p == std::string::npos) return {};
    size_t q1 = obj.find('"', p + 1); if (q1 == std::string::npos) return {};
    size_t q2 = obj.find('"', q1 + 1); if (q2 == std::string::npos) return {};
    return obj.substr(q1 + 1, q2 - q1 - 1);
}

static std::string findMatching(const std::string& s, size_t start, char openC, char closeC) {
    if (start >= s.size() || s[start] != openC) return {};
    int depth = 0; bool inStr = false;
    for (size_t i = start; i < s.size(); i++) {
        char c = s[i];
        if (inStr) { if (c == '\\') { i++; continue; } if (c == '"') inStr = false; }
        else { if (c == '"') inStr = true; else if (c == openC) depth++; else if (c == closeC) { depth--; if (depth == 0) return s.substr(start, i - start + 1); } }
    }
    return {};
}

static std::vector<Shape> parseShapes(const std::string& text) {
    std::vector<Shape> out;
    size_t pos = text.find("\"shapes\"");
    if (pos == std::string::npos) return out;
    size_t arrStart = text.find('[', pos);
    if (arrStart == std::string::npos) return out;
    std::string arr = findMatching(text, arrStart, '[', ']');
    if (arr.empty()) return out;
    size_t i = 1;
    while (i < arr.size()) {
        while (i < arr.size() && (std::isspace((unsigned char)arr[i]) || arr[i] == ',')) i++;
        if (i >= arr.size() || arr[i] == ']') break;
        if (arr[i] != '{') { i++; continue; }
        std::string obj = findMatching(arr, i, '{', '}');
        if (obj.empty()) break;
        Shape sh;
        sh.kind = getStr(obj, "kind");
        sh.z = getFloat(obj, "z");
        sh.rgba = getUint(obj, "rgba");
        sh.r = getFloat(obj, "r");
        size_t pp = obj.find("\"pts\"");
        if (pp != std::string::npos) {
            size_t ab = obj.find('[', pp);
            if (ab != std::string::npos) {
                std::string ptsArr = findMatching(obj, ab, '[', ']');
                std::regex re(R"(\[\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\])");
                auto begin = std::sregex_iterator(ptsArr.begin(), ptsArr.end(), re);
                auto end = std::sregex_iterator();
                for (auto it = begin; it != end; ++it) {
                    Pt2 p; p.x = std::stof((*it)[1].str()); p.y = std::stof((*it)[2].str()); sh.pts.push_back(p);
                }
            }
        }
        out.push_back(sh);
        i += obj.size();
    }
    return out;
}

#include <glad/gl.h>   // must come before glfw3.h
#include <GLFW/glfw3.h>
#include <iostream>

const char* vsSrc = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;
out vec3 vColor;
void main() { vColor = aColor; gl_Position = vec4(aPos, 0.0, 1.0); })";

const char* fsSrc = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); })";

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(s, 512, nullptr, log); std::cerr << log << "\n"; }
    return s;
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Bench Years Viewer", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) return -1;
    std::cout << glGetString(GL_VERSION) << "\n" << glGetString(GL_RENDERER) << "\n";

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); });

    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    float verts[] = {  // x, y, r, g, b
        -0.5f, -0.5f, 1, 0, 0,
         0.5f, -0.5f, 0, 1, 0,
         0.0f,  0.5f, 0, 0, 1 };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
struct Mat4 { float m[16]; };

static Mat4 identity() {
    Mat4 r{}; for (int i = 0; i < 16; i++) r.m[i] = 0; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r;
}
static Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r{}; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { float s = 0; for (int k = 0; k < 4; k++) s += a.m[i*4+k] * b.m[k*4+j]; r.m[i*4+j] = s; } return r;
}
static Mat4 perspective(float fovyDeg, float aspect, float znear, float zfar) {
    float f = 1.f / std::tan(fovyDeg * 3.14159265f / 180.f / 2.f);
    Mat4 r{}; for (int i = 0; i < 16; i++) r.m[i] = 0;
    r.m[0] = f / aspect; r.m[5] = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.f;
    r.m[14] = (2.f * zfar * znear) / (znear - zfar);
    return r;
}
static Mat4 lookAt(float ex, float ey, float ez, float cx, float cy, float cz, float ux, float uy, float uz) {
    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float fl = std::sqrt(fx*fx + fy*fy + fz*fz); fx/=fl; fy/=fl; fz/=fl;
    float sx = fy*uz - fz*uy, sy = fz*ux - fx*uz, sz = fx*uy - fy*ux;
    float sl = std::sqrt(sx*sx+sy*sy+sz*sz); sx/=sl; sy/=sl; sz/=sl;
    float ux2 = sy*fz - sz*fy, uy2 = sz*fx - sx*fz, uz2 = sx*fy - sy*fx;
    Mat4 r = identity();
    r.m[0]=sx; r.m[1]=ux2; r.m[2]=-fx; r.m[3]=0;
    r.m[4]=sy; r.m[5]=uy2; r.m[6]=-fy; r.m[7]=0;
    r.m[8]=sz; r.m[9]=uz2; r.m[10]=-fz; r.m[11]=0;
    r.m[12]=-(sx*ex+sy*ey+sz*ez);
    r.m[13]=-(ux2*ex+uy2*ey+uz2*ez);
    r.m[14]=(fx*ex+fy*ey+fz*ez);
    r.m[15]=1;
    return r;
}

static const char* vsSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
out vec3 vColor;
uniform mat4 uMVP;
void main() { vColor = aColor; gl_Position = uMVP * vec4(aPos, 1.0); })";

static const char* fsSrc = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); })";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(s, 512, nullptr, log); std::cerr << log << "\n"; }
    return s;
}

static void colorFromRgba(unsigned int rgba, float& r, float& g, float& b) {
    r = (rgba & 0xFF) / 255.f;
    g = ((rgba >> 8) & 0xFF) / 255.f;
    b = ((rgba >> 16) & 0xFF) / 255.f;
}

static void addPolyVerts(std::vector<float>& v, const std::vector<Pt2>& pts, float z, float cr, float cg, float cb) {
    if (pts.size() < 3) return;
    for (size_t i = 1; i + 1 < pts.size(); i++) {
        const Pt2* arr[3] = { &pts[0], &pts[i], &pts[i+1] };
        for (int k = 0; k < 3; k++) {
            v.push_back(arr[k]->x); v.push_back(360.f - arr[k]->y); v.push_back(z);
            v.push_back(cr); v.push_back(cg); v.push_back(cb);
        }
    }
}

static void addDiscVerts(std::vector<float>& v, float cx, float cy, float r, float z, float cr, float cg, float cb) {
    const int segs = 32;
    std::vector<Pt2> ring;
    for (int i = 0; i < segs; i++) {
        float a = (float)i / segs * 6.2831853f;
        ring.push_back({ cx + std::cos(a) * r, cy + std::sin(a) * r });
    }
    for (int i = 0; i < segs; i++) {
        const Pt2& p0 = ring[i];
        const Pt2& p1 = ring[(i + 1) % segs];
        v.push_back(cx); v.push_back(360.f - cy); v.push_back(z); v.push_back(cr); v.push_back(cg); v.push_back(cb);
        v.push_back(p0.x); v.push_back(360.f - p0.y); v.push_back(z); v.push_back(cr); v.push_back(cg); v.push_back(cb);
        v.push_back(p1.x); v.push_back(360.f - p1.y); v.push_back(z); v.push_back(cr); v.push_back(cg); v.push_back(cb);
    }
}
int main(int argc, char** argv) {
    std::string jsonPath = "out/scene0.json";
    if (argc > 1) jsonPath = argv[1];

    std::string text = readFile(jsonPath);
    if (text.empty()) { std::cerr << "Could not read " << jsonPath << "\n"; return 1; }
    auto shapes = parseShapes(text);
    std::cout << "Loaded " << shapes.size() << " shapes from " << jsonPath << "\n";

    std::vector<float> verts;
    for (const auto& sh : shapes) {
        float cr, cg, cb; colorFromRgba(sh.rgba, cr, cg, cb);
        float z = sh.z * 4.0f;  // To add more depth -> 8, to reduce the depth -> 2
        if (sh.kind == "disc") {
            float cx = sh.pts.empty() ? 0.f : sh.pts[0].x;
            float cy = sh.pts.empty() ? 0.f : sh.pts[0].y;
            addDiscVerts(verts, cx, cy, sh.r, z, cr, cg, cb);
        } else if (sh.kind == "line") {
            for (size_t i = 0; i + 1 < sh.pts.size(); i++) {
                verts.push_back(sh.pts[i].x); verts.push_back(360.f - sh.pts[i].y); verts.push_back(z);
                verts.push_back(cr); verts.push_back(cg); verts.push_back(cb);
                verts.push_back(sh.pts[i+1].x); verts.push_back(360.f - sh.pts[i+1].y); verts.push_back(z);
                verts.push_back(cr); verts.push_back(cg); verts.push_back(cb);
            }
        } else {
            addPolyVerts(verts, sh.pts, z, cr, cg, cb);
        }
    }

    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(960, 540, "Bench Years 3D Viewer", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) return -1;
    std::cout << glGetString(GL_VERSION) << "\n" << glGetString(GL_RENDERER) << "\n";

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, 960, 540);

    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    float aspect = 960.f / 540.f;
    Mat4 proj = perspective(45.f, aspect, 0.1f, 1000.f);
    Mat4 view = lookAt(320.f, 180.f, -260.f, 320.f, 180.f, 8.f, 0.f, 1.f, 0.f);
    Mat4 mvp = mul(proj, view);

    GLint mvpLoc = glGetUniformLocation(prog, "uMVP");

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        glClearColor(0.08f, 0.1f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 6));

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

