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
    unsigned int vbo[4]; // position, texture coord, normal, element
    unsigned int texture;
    unsigned int indices;

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
std::vector<object_struct> objects; // vertex array object,vertex buffer object and texture(color) for objects

static void error_callback(int error, const char *description) {
    fputs(description, stderr);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    // exit program when esc pressed
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

static unsigned int setup_shader(const char *vertex_shader, const char *fragment_shader) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_shader, nullptr);
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

    // create a shader object and set shader type to run on a programmable fragment processor
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_shader, nullptr);
    glCompileShader(fs);

    // check if the shader created successfully
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

    // attach vertex, fragment shader to program
    glAttachShader(program, vs);
    glAttachShader(program, fs);

    glLinkProgram(program);

    // check if the program created successfully
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

    // load model
    std::string err = tinyobj::LoadObj(shapes, materials, filename);

    if (!err.empty() || shapes.size() == 0) {
        std::cerr << err << std::endl;
        exit(1);
    }

    new_node.indices = (unsigned int) shapes[0].mesh.indices.size();
    new_node.program = program;

    // get the vertex array index
    glGenVertexArrays(1, &new_node.vao);
    glGenBuffers(sizeof(new_node.vbo) / sizeof(new_node.vbo[0]), new_node.vbo);
    glGenTextures(1, &new_node.texture);

    glBindVertexArray(new_node.vao);

    // upload position array
    glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * shapes[0].mesh.positions.size(),
                 shapes[0].mesh.positions.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);                          // generic vertex attribute index
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0); // define an array of generic vertex attribute data

    if (shapes[0].mesh.texcoords.size() > 0) {
        // upload texCoord array
        glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[1]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * shapes[0].mesh.texcoords.size(),
                     shapes[0].mesh.texcoords.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

        // setup texture
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
        // upload normal array
        glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[2]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * shapes[0].mesh.normals.size(),
                     shapes[0].mesh.normals.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }

    // setup index buffer for glDrawElements
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, new_node.vbo[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * new_node.indices,
                 shapes[0].mesh.indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    objects.push_back(new_node);
    return (unsigned int) (objects.size() - 1);
}

static void releaseObjects() {
    for (int i = 0; i < objects.size(); i++) {
        glDeleteVertexArrays(1, &objects[i].vao);
        glDeleteTextures(1, &objects[i].texture);
        // release buffer and gl program
        glDeleteBuffers(sizeof(objects[i].vbo) / (objects[i].vbo[0]), objects[i].vbo);
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
        // bind texture
        glBindTexture(GL_TEXTURE_2D, objects[i].texture);
        glDrawElements(GL_TRIANGLES, objects[i].indices, GL_UNSIGNED_INT, nullptr);
        // send newly calculated vp to shader
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

    float sun_angle = angle / 8;
    float earth_angle = angle * 3;
    float moon_angle = earth_angle * (30.0f / 27);

    float earth_orbit = 3 * glm::radians(angle);
    const int earth_orbit_x = 30;
    const int earth_orbit_y = 50;
    float moon_orbit = 6 * glm::radians(angle);
    const int moon_orbit_x = 10;
    const int moon_orbit_y = 8;

    // sun
    objects[sun_index].rotation = glm::rotate(glm::mat4(), sun_angle, glm::vec3(0.0f, 0.0f, 1.0f));
    objects[sun_index].model = buildTransform(objects[sun_index]);

    // earth
    objects[earth_index].translation = glm::translate(glm::mat4(),
                                                      glm::vec3(cos(earth_orbit) * earth_orbit_x, 0.0f,
                                                                sin(-earth_orbit) * earth_orbit_y));
    objects[earth_index].rotation = glm::rotate(glm::mat4(), earth_angle, glm::vec3(0.0f, 0.0f, 1.0f));
    objects[earth_index].model = buildTransform(objects[earth_index]);

    // moon
    objects[moon_index].origin = objects[earth_index].translation; // set earth position as origin
    objects[moon_index].translation = glm::translate(glm::mat4(),
                                                     glm::vec3(cos(moon_orbit) * moon_orbit_x, 0.0f,
                                                               sin(-moon_orbit) * moon_orbit_y));
    objects[moon_index].rotation = glm::rotate(glm::mat4(), moon_angle, glm::vec3(0.0f, 0.0f, 1.0f));
    objects[moon_index].model = buildTransform(objects[moon_index]);

    if (angle >= 360) angle = 0;
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
    objects[bloom_index].base_model = glm::scale(glm::mat4(), glm::vec3(0.4f, 0.5f, 0.0f)); // resize bloom: x, y, z
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

    // make our window context the main context on current thread
    glfwMakeContextCurrent(window);

    // this line MUST put below glfwMakeContextCurrent
    glewExperimental = GL_TRUE;
    glewInit();

    // enable vsync
    glfwSwapInterval(1);

    // setup input callback
    glfwSetKeyCallback(window, key_callback);

    // alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // depth comparisons and update the depth buffer
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);

    // init objects
    setupObjects();

    float last;
    last = (float) glfwGetTime();
    int fps = 0;

    // gl program will keep draw here until you close the window
    while (!glfwWindowShouldClose(window)) {
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
