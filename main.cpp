#define GLM_FORCE_RADIANS

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <tiny_obj_loader.h>

typedef struct obj {
    unsigned int program;
    unsigned int vao;
    unsigned int vbo[4];
    unsigned int texture;

    // transform
    glm::mat4 origin;
    glm::mat4 translation;
    glm::mat4 rotation;
    glm::mat4 base_model;
    glm::mat4 model;

    obj() : origin(glm::mat4()),
            model(glm::mat4(1.0f)),
            base_model(glm::mat4(1.0f)),
            rotation(glm::mat4()),
            translation(glm::mat4()) { }
} object_struct;

unsigned int sun_index, earth_index, moon_index; // program index
std::vector<object_struct> objects; // vertex array object,vertex buffer object and texture(color) for objs
std::vector<int> indicesCount;      // number of indices of objects

static void error_callback(int error, const char *description) {
    fputs(description, stderr);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

static unsigned int setup_shader(const char *vertex_shader, const char *fragment_shader) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, (const GLchar **) &vertex_shader, nullptr);

    glCompileShader(vs);

    int status, maxLength;
    char *infoLog = nullptr;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &maxLength);

        /* The maxLength includes the NULL character */
        infoLog = new char[maxLength];

        glGetShaderInfoLog(vs, maxLength, &maxLength, infoLog);

        fprintf(stderr, "Vertex Shader Error: %s\n", infoLog);

        /* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
        /* In this simple program, we'll just leave */
        delete[] infoLog;
        return 0;
    }

    //create a shader object and set shader type to run on a programmable fragment processor
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, (const GLchar **) &fragment_shader, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &maxLength);

        /* The maxLength includes the NULL character */
        infoLog = new char[maxLength];

        glGetShaderInfoLog(fs, maxLength, &maxLength, infoLog);

        fprintf(stderr, "Fragment Shader Error: %s\n", infoLog);

        /* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
        /* In this simple program, we'll just leave */
        delete[] infoLog;
        return 0;
    }

    unsigned int program = glCreateProgram();
    // Attach our shaders to our program
    glAttachShader(program, vs);
    glAttachShader(program, fs);

    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status == GL_FALSE) {
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);


        /* The maxLength includes the NULL character */
        infoLog = new char[maxLength];
        glGetProgramInfoLog(program, maxLength, NULL, infoLog);

        glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);

        fprintf(stderr, "Link Error: %s\n", infoLog);

        /* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
        /* In this simple program, we'll just leave */
        delete[] infoLog;
        return 0;
    }
    return program;
}

static std::string readfile(const char *filename) {
    std::ifstream ifs(filename);
    if (!ifs)
        exit(EXIT_FAILURE);
    return std::string((std::istreambuf_iterator<char>(ifs)),
                       (std::istreambuf_iterator<char>()));
}

// mini bmp loader written by HSU YOU-LUN
static unsigned char *load_bmp(const char *bmp, unsigned int *width, unsigned int *height, unsigned short int *bits) {
    unsigned char *result = nullptr;
    FILE *fp = fopen(bmp, "rb");
    if (!fp)
        return nullptr;
    char type[2];
    unsigned int size, offset;
    // check for magic signature
    fread(type, sizeof(type), 1, fp);
    if (type[0] == 0x42 || type[1] == 0x4d) {
        fread(&size, sizeof(size), 1, fp);
        // ignore 2 two-byte reversed fields
        fseek(fp, 4, SEEK_CUR);
        fread(&offset, sizeof(offset), 1, fp);
        // ignore size of bmpinfoheader field
        fseek(fp, 4, SEEK_CUR);
        fread(width, sizeof(*width), 1, fp);
        fread(height, sizeof(*height), 1, fp);
        // ignore planes field
        fseek(fp, 2, SEEK_CUR);
        fread(bits, sizeof(*bits), 1, fp);
        unsigned char *pos = result = new unsigned char[size - offset];
        fseek(fp, offset, SEEK_SET);
        while (size - ftell(fp) > 0)
            pos += fread(pos, 1, size - ftell(fp), fp);
    }
    fclose(fp);
    return result;
}

static unsigned int add_obj(unsigned int program, const char *filename, const char *texbmp) {
    object_struct new_node;

    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string err = tinyobj::LoadObj(shapes, materials, filename);

    if (!err.empty() || shapes.size() == 0) {
        std::cerr << err << std::endl;
        exit(1);
    }

    glGenVertexArrays(1, &new_node.vao);
    glGenBuffers(4, new_node.vbo);
    glGenTextures(1, &new_node.texture);

    glBindVertexArray(new_node.vao);

    // Upload position array
    glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * shapes[0].mesh.positions.size(),
                 shapes[0].mesh.positions.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    if (shapes[0].mesh.texcoords.size() > 0) {

        // Upload texCoord array
        glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[1]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * shapes[0].mesh.texcoords.size(),
                     shapes[0].mesh.texcoords.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

        glBindTexture(GL_TEXTURE_2D, new_node.texture);
        unsigned int width, height;
        unsigned short int bits;
        unsigned char *bgr = load_bmp(texbmp, &width, &height, &bits);
        GLenum format = (bits == 24 ? GL_BGR : GL_BGRA);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, format, GL_UNSIGNED_BYTE, bgr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glGenerateMipmap(GL_TEXTURE_2D);
        delete[] bgr;
    }

    if (shapes[0].mesh.normals.size() > 0) {
        // Upload normal array
        glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[2]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * shapes[0].mesh.normals.size(),
                     shapes[0].mesh.normals.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }

    // Setup index buffer for glDrawElements
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, new_node.vbo[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * shapes[0].mesh.indices.size(),
                 shapes[0].mesh.indices.data(), GL_STATIC_DRAW);

    indicesCount.push_back(shapes[0].mesh.indices.size());

    glBindVertexArray(0);

    new_node.program = program;

    objects.push_back(new_node);
    return (unsigned int) (objects.size() - 1);
}

static void releaseObjects() {
    for (int i = 0; i < objects.size(); i++) {
        glDeleteVertexArrays(1, &objects[i].vao);
        glDeleteTextures(1, &objects[i].texture);
        glDeleteBuffers(4, objects[i].vbo);
        // release gl program
        glDeleteProgram(objects[i].program);
    }
}

static void setUniformMat4(unsigned int program, const std::string &name, const glm::mat4 &mat) {
    // This line can be ignore. But, if you have multiple shader program
    // You must check if current binding is the one you want
    glUseProgram(program);
    GLint loc = glGetUniformLocation(program, name.c_str());
    if (loc == -1) return;

    // mat4 of glm is column major, same as opengl
    // we don't need to transpose it. so..GL_FALSE
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(mat));
}

static void render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (int i = 0; i < objects.size(); i++) {
        glUseProgram(objects[i].program);
        glBindVertexArray(objects[i].vao);
        glBindTexture(GL_TEXTURE_2D, objects[i].texture);
        // you should send some data to shader here
        glDrawElements(GL_TRIANGLES, indicesCount[i], GL_UNSIGNED_INT, nullptr);
        setUniformMat4(objects[i].program, "vp", objects[i].model);
    }
    glBindVertexArray(0);
}

// calculate transform before rendering
glm::mat4 buildTransform(object_struct obj) {
    return obj.base_model *
           obj.origin *
           obj.translation *
           obj.rotation;
}

// being called at certain interval
void rotateObject() {
    static float angle = 0;
    angle += 0.2f;
    if (angle == 360) angle = 0;
    float orbit = glm::radians(angle * 3);

    // sun
    objects[sun_index].rotation = glm::rotate(glm::mat4(), angle / 8, glm::vec3(0.0f, 0.0f, 1.0f));
    objects[sun_index].model = buildTransform(objects[sun_index]);

    // earth
    objects[earth_index].translation =
            glm::translate(glm::mat4(), glm::vec3(cos(orbit) * 30, 0.0f, sin(-orbit) * 50));
    objects[earth_index].rotation = glm::rotate(glm::mat4(), angle * 3, glm::vec3(0.0f, 0.0f, 1.0f));
    objects[earth_index].model = buildTransform(objects[earth_index]);

    // moon
    objects[moon_index].origin = objects[earth_index].translation; // set earth position as origin
    objects[moon_index].translation =
            glm::translate(glm::mat4(), glm::vec3(cos(orbit * 3) * 10, 0.0f, sin(-orbit * 3) * 8));
    objects[moon_index].rotation = glm::rotate(glm::mat4(), angle * 3 * (30.0f / 27), glm::vec3(0.0f, 0.0f, 1.0f));
    objects[moon_index].model = buildTransform(objects[moon_index]);
}

// initial objects
void setupObjects() {
    // load shader sun_program
    unsigned int sun_program = setup_shader(readfile("shader/vs.txt").c_str(), readfile("shader/fs.txt").c_str());
    unsigned int earth_program = setup_shader(readfile("shader/vs.txt").c_str(), readfile("shader/fs.txt").c_str());
    unsigned int moon_program = setup_shader(readfile("shader/vs.txt").c_str(), readfile("shader/fs.txt").c_str());
    unsigned int bloom_program = setup_shader(readfile("shader/vs.txt").c_str(), readfile("shader/fs.txt").c_str());

    sun_index = add_obj(sun_program, "render/sun.obj", "render/sun.bmp");
    earth_index = add_obj(earth_program, "render/earth.obj", "render/earth.bmp");
    moon_index = add_obj(moon_program, "render/earth.obj", "render/moon.bmp");
    unsigned int bloom_index = add_obj(bloom_program, "render/rectangle.obj", "render/bloom.bmp");

    // base transform: sun
    objects[sun_index].base_model =
            glm::perspective(glm::radians(45.0f), 800.0f / 600, 1.0f, 170.0f) *
            glm::lookAt(glm::vec3(1.0f, 40.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
    objects[sun_index].origin = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, 0.0f));

    // base transform: bloom
    objects[bloom_index].base_model = glm::scale(glm::mat4(), glm::vec3(0.4f, 0.5f, 0.0f));
    objects[bloom_index].origin = objects[sun_index].origin;
    objects[bloom_index].model = buildTransform(objects[bloom_index]);

    // base transform: earth
    objects[earth_index].base_model =
            glm::perspective(glm::radians(45.0f), 800.0f / 600, 1.0f, 170.0f) *
            glm::lookAt(glm::vec3(1.0f, 110.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
    objects[earth_index].origin = objects[sun_index].origin;

    // base transform: moon
    objects[moon_index].base_model =
            glm::perspective(glm::radians(45.0f), 800.0f / 600, 1.0f, 170.0f) *
            glm::lookAt(glm::vec3(1.0f, 110.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
}

int main(int argc, char *argv[]) {
    GLFWwindow *window;
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        exit(EXIT_FAILURE);
    // OpenGL 3.3, Mac OS X is reported to have some problem. However I don't have Mac to test
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    // For Mac OS X
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(800, 600, "Simple Example", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // Make our window context the main context on current thread
    glfwMakeContextCurrent(window);

    // This line MUST put below glfwMakeContextCurrent
    glewExperimental = GL_TRUE;
    glewInit();

    // Enable vsync
    glfwSwapInterval(1);

    // Setup input callback
    glfwSetKeyCallback(window, key_callback);

    // Alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);

    // init objects
    setupObjects();

    float last;
    last = (float) glfwGetTime();
    int fps = 0;
    while (!glfwWindowShouldClose(window)) {//sun_program will keep draw here until you close the window
        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
        fps++;
        if (glfwGetTime() - last > 0.01) {
            // rotate sun, earth, moon
            rotateObject();
            std::cout << (double) fps / (glfwGetTime() - last) << std::endl;
            fps = 0;
            last = (float) glfwGetTime();
        }
    }

    releaseObjects();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
