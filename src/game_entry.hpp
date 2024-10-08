#ifndef GAME_ENTRY_HPP
#define GAME_ENTRY_HPP

#include "glad/glad.h"

#include "GLFW/glfw3.h"

#include "lib/glm/glm.hpp"
#include "lib/glm/gtc/matrix_transform.hpp"
#include "lib/glm/gtc/type_ptr.hpp"
#include "lib/stb/stb_image.h"

#include "iostream"

#include "defines.h"

#include "camera.hpp"
#include "data.hpp"
#include "game_object.hpp"
#include "lighting.hpp"
#include "material.hpp"
#include "model.hpp"
#include "shader.hpp"

struct GameConfig {
    u16 window_width;
    u16 window_height;
    char *window_name;
};

struct GameState {
    GLFWwindow *window;
    u16 window_width;
    u16 window_height;
    real64 last_time;
    real32 delta_time;
    bool is_running;
    Camera player_camera;
};
// TODO: maybe add game/engine settings here

static GameState GState;
static bool is_initialized = false;

static bool global_first_mouse = true;
static real32 global_lastX;
static real32 global_lastY;

void framebuffer_size_callback(GLFWwindow *window, i32 width, i32 height);
void process_input(GLFWwindow *window);
void mouse_callback(GLFWwindow *window, real64 in_pos_x, real64 in_pos_y);
void scroll_callback(GLFWwindow *window, real64 offsetX, real64 offsetY);
void game_shutdown();

bool game_init(GameConfig *cfg) {
    if (is_initialized) {
        std::cout << "error. application already created" << std::endl;
        return false;
    }

    if (!glfwInit()) {
        std::cout << "failed to initialize glfw" << std::endl;
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow *window = glfwCreateWindow(cfg->window_width, cfg->window_height, cfg->window_name, NULL, NULL);
    if (window == NULL) {
        std::cout << "failed to create window";
        return false;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "failed to initialize glad" << std::endl;
        return false;
    }
    glViewport(0, 0, cfg->window_width, cfg->window_height);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glEnable(GL_DEPTH_TEST);

    GState.is_running = true;
    GState.window_width = cfg->window_width;
    GState.window_height = cfg->window_height;
    GState.last_time = 0.0f;
    GState.delta_time = 0.0f;
    GState.window = window;
    GState.player_camera = Camera();

    global_lastX = GState.window_width / 2.0f;
    global_lastY = GState.window_height / 2.0f;

    is_initialized = true;
    return true;
}

void game_run() {
    ShaderProgram lighting_shader("src/shaders/lighting_vs.glsl", "src/shaders/lighting_fs.glsl");
    ShaderProgram skybox_shader("src/shaders/skybox_vs.glsl", "src/shaders/skybox_fs.glsl");
    ShaderProgram material_shader("src/shaders/material_vs.glsl", "src/shaders/material_fs.glsl");
    ShaderProgram shadow_shader("src/shaders/shadow_map_vs.glsl", "src/shaders/shadow_map_fs.glsl");

    BasicMesh skybox_mesh;
    load_skybox_mesh(skybox_mesh, sizeof(SKYBOX_VERTICES), &SKYBOX_VERTICES[0]);
    std::string cubemap_faces[] = {
        "skybox/right.jpg",
        "skybox/left.jpg",
        "skybox/top.jpg",
        "skybox/bottom.jpg",
        "skybox/front.jpg",
        "skybox/back.jpg",
    };
    GLuint skybox_texture = load_cubemap(cubemap_faces, 6);

    Material container_mat;
    load_material(container_mat, "container2.png", "container2_s.png", 128);
    BasicMesh container_mesh;
    load_basic_mesh(container_mesh, sizeof(CUBE_VERTICES), &CUBE_VERTICES[0]);

    BasicMesh wall_mesh;
    load_basic_mesh(wall_mesh, sizeof(CUBE_VERTICES), &CUBE_VERTICES[0]);
    Material wall_mat;
    load_material(wall_mat, "brickwall.jpg", "brickwall.jpg", 128, "brickwall_n.jpg");

    BasicMesh plane_mesh;
    load_basic_mesh(plane_mesh, sizeof(PLANE_VERTICES), PLANE_VERTICES);
    Material plane_mat;
    load_material(plane_mat, "wood.png");

    GLuint light_cube_VAO, light_cube_VBO;
    glGenVertexArrays(1, &light_cube_VAO);
    glGenBuffers(1, &light_cube_VBO);
    glBindVertexArray(light_cube_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, light_cube_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(LIGHT_CUBE_VERTICES), LIGHT_CUBE_VERTICES, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(real32), (void *)0);

    DirectionalLight dir_light;
    dir_light.direction = glm::vec3(-0.2f, -1.0f, -0.3f);

    u8 point_light_count = 3;
    PointLight point_lights[MAX_POINT_LIGHT_COUNT];
    glm::vec3 plight_positions[3] = {
        glm::vec3(1.2f, 1.0f, 2.0f),
        glm::vec3(1.2f, 3.0f, 1.0f),
        glm::vec3(1.2f, -3.0f, 1.0f),
    };
    load_point_lights(point_lights, plight_positions, point_light_count);

    GameObject backpack = GameObject(material_shader, "backpack/backpack.obj");
    backpack.move(glm::vec3(2.0f, -2.0f, -4.0f));
    backpack.scale(glm::vec3(0.25f, 0.25f, 0.25f));
    while (GState.is_running) {
        real32 current_time = glfwGetTime();
        GState.delta_time = current_time - GState.last_time;
        GState.last_time = current_time;

        process_input(GState.window);

        glClearColor(0, 0, 0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = GState.player_camera.perspective_projection(GState.window_width, GState.window_height);
        glm::mat4 view = GState.player_camera.view_matrix();
        glm::mat4 projection_mul_view = projection * view;
        glm::vec3 player_position = GState.player_camera.position;

        lighting_shader.use();
        for (u8 i = 0; i < point_light_count; i++) {
            glm::mat4 transform = glm::mat4(1.0f);
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, plight_positions[i]);
            model = glm::scale(model, glm::vec3(0.2f));
            transform = projection * view * model * transform;
            lighting_shader.set_mat4("transform", transform);
            glBindVertexArray(light_cube_VAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        material_shader.use();
        material_shader.set_mat4("projection_mul_view", projection_mul_view);
        material_shader.set_vec3("viewer_position", player_position);
        set_shader_lighting_data(material_shader, dir_light, point_lights, point_light_count);

        glm::mat4 model = glm::mat4(1.0f);
        glm::mat3 normal_matrix = glm::mat3(model);
        normal_matrix = glm::inverse(normal_matrix);
        normal_matrix = glm::transpose(normal_matrix);
        material_shader.set_mat3("normal_matrix", normal_matrix);
        material_shader.set_mat4("model", model);
        draw_basic_mesh(container_mesh, material_shader, container_mat);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(2.0f, 2.0f, 2.0f));
        material_shader.set_mat4("model", model);
        normal_matrix = glm::mat3(model);
        normal_matrix = glm::inverse(normal_matrix);
        normal_matrix = glm::transpose(normal_matrix);
        material_shader.set_mat3("normal_matrix", normal_matrix);
        draw_basic_mesh(wall_mesh, material_shader, wall_mat);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -2.0f, 0.0f));
        material_shader.set_mat4("model", model);
        normal_matrix = glm::mat3(model);
        normal_matrix = glm::inverse(normal_matrix);
        normal_matrix = glm::transpose(normal_matrix);
        material_shader.set_mat3("normal_matrix", normal_matrix);
        draw_basic_mesh(plane_mesh, material_shader, plane_mat);

        backpack.render();

        skybox_shader.use();
        view = glm::mat4(glm::mat3(GState.player_camera.view_matrix()));
        projection_mul_view = projection * view;
        skybox_shader.set_mat4("projection_mul_view", projection_mul_view);
        draw_skybox(skybox_mesh, skybox_shader, skybox_texture);

        glfwSwapBuffers(GState.window);
        glfwPollEvents();
    }

    GState.is_running = false;
    game_shutdown();
}

void game_shutdown() {
    glfwTerminate();
}

// CALLBACKS
void framebuffer_size_callback(GLFWwindow *window, i32 width, i32 height) {
    glViewport(0, 0, width, height);
    GState.window_width = width;
    GState.window_height = height;
}

void process_input(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        GState.is_running = false;
    }
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        GState.player_camera.move(Camera::FORWARD, GState.delta_time);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        GState.player_camera.move(Camera::BACKWARD, GState.delta_time);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        GState.player_camera.move(Camera::LEFT, GState.delta_time);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        GState.player_camera.move(Camera::RIGHT, GState.delta_time);
    }
}

void mouse_callback(GLFWwindow *window, real64 in_pos_x, real64 in_pos_y) {
    real32 posX = (real32)in_pos_x;
    real32 posY = (real32)in_pos_y;
    if (global_first_mouse) {
        global_lastX = posX;
        global_lastY = posY;
        global_first_mouse = false;
    }
    real32 offsetX = posX - global_lastX;
    real32 offsetY = global_lastY - posY;
    global_lastX = posX;
    global_lastY = posY;
    GState.player_camera.process_mouse(offsetX, offsetY);
}

void scroll_callback(GLFWwindow *window, real64 offsetX, real64 offsetY) {
    GState.player_camera.process_scroll((real32)offsetY);
}

#endif