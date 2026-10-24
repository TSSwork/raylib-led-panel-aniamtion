#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <raylib.h>
#include <rlgl.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 800

Color priamry = {230, 41, 55, 255};
Color secondary = {10, 0, 0, 255};
Font clock_font;
float clock_radius = (float)SCREEN_WIDTH/2 - 200;

int digits[10][7] = {
    {1,1,1,0,1,1,1}, //0
    {0,0,1,0,0,1,0}, //1
    {1,0,1,1,1,0,1}, //2
    {1,0,1,1,0,1,1}, //3
    {0,1,1,1,0,1,0}, //4
    {1,1,0,1,0,1,1}, //5
    {1,1,0,1,1,1,1}, //6
    {0,0,1,0,0,1,1}, //7
    {1,1,1,1,1,1,1}, //8
    {1,1,1,1,0,1,1}, //9
};

void draw_digit_hand(Vector2 center,int length, int height, int triangle_height, int angle, bool on){
    float half_l = length / 2.0f;
    float half_h = height / 2.0f;
    float end_l =  half_l + triangle_height;

    Vector2 a = { center.x - end_l , center.y }; // 0
    Vector2 b = { center.x - half_l , center.y + half_h };  // 1
    Vector2 c = { center.x - half_l, center.y - half_h }; // 2
    Vector2 d = { center.x + half_l, center.y + half_h }; // 3
    Vector2 e = { center.x + half_l, center.y - half_h }; // 4
    Vector2 f = { center.x + end_l, center.y }; // 5

    Vector2 points[] = { a, b, c, d, e, f };
    int count = sizeof(points)/sizeof(points[0]);

    rlPushMatrix();
        rlTranslatef(center.x, center.y, 0);
        rlRotatef(angle, 0, 0, 1);
        rlTranslatef(-center.x, -center.y, 0);
        DrawTriangleStrip(points, count, on ? priamry : secondary);
    rlPopMatrix();
}

void draw_full_number(Vector2 position, int digit) {

    int *digit_segments = digits[digit];

    int length = 50;
    int height = 20;
    int triangle_height = 10;
    int full_height = length + (triangle_height * 2);
    int half_height = full_height / 2;
    // top
    draw_digit_hand((Vector2){position.x, position.y + full_height + 8}, length, height, triangle_height, 0, digit_segments[0]);
    // top left
    draw_digit_hand((Vector2){position.x - half_height - 4, position.y - half_height - 4}, length, height, triangle_height, 90, digit_segments[1]);
    // top right
    draw_digit_hand((Vector2){position.x + half_height + 4, position.y - half_height - 4}, length, height, triangle_height, 90, digit_segments[2]);
    // center
    draw_digit_hand((Vector2){position.x, position.y}, length, height, triangle_height, 0, digit_segments[3]);
    // bottom left
    draw_digit_hand((Vector2){position.x - half_height - 4, position.y + half_height + 4}, length, height, triangle_height, 90, digit_segments[4]);
    // bottom right
    draw_digit_hand((Vector2){position.x + half_height + 4, position.y + half_height + 4}, length, height, triangle_height, 90, digit_segments[5]);
    //bottom
    draw_digit_hand((Vector2){position.x, position.y - full_height - 8}, length, height, triangle_height, 0, digit_segments[6]);

}

void draw_separator(Vector2 position) {
    DrawRectangle(position.x, position.y - 20, 15, 15, RED);
    DrawRectangle(position.x, position.y + 10, 15, 15, RED);
}

void analog_face(Vector2 position, float radius) {
    //Face
    DrawCircle(position.x, position.y, radius, WHITE);
    // Markers
    for (int i = 0; i < 60; i++) {

        int length = 10 + (i % 5 == 0 ? 10 : 0);
        float angle = i * 6.0f * DEG2RAD - PI/2;

        float end_x = position.x + cos(angle) * (radius - 10);
        float end_y = position.y + sin(angle) * (radius - 10);
        float start_x = position.x + cos(angle) * (radius - 10 - length);
        float start_y = position.y + sin(angle) * (radius - 10 - length);
        float number_x = position.x + cos(angle) * (radius + 20);
        float number_y = position.y + sin(angle) * (radius + 20);

        // Minute Lines
        DrawLineEx((Vector2){start_x, start_y}, (Vector2){end_x, end_y}, 2.0f, BLACK);

        // Primary 12
        if (i % 5 == 0) {
            int hour = i / 5;
            if (hour == 0) hour = 12;

            const char* text = TextFormat("%d", hour);
            float fontSize = 30.0f;
            float spacing = 0.0f;

            // 2. Calculate text dimensions to center it properly
            Vector2 textSize = MeasureTextEx(clock_font, text, fontSize, spacing);

            // Adjust coordinates so the CENTER of the text is at number_x/y
            Vector2 finalPos = {
                number_x - textSize.x / 2.0f,
                number_y - textSize.y / 2.0f
            };

            DrawTextEx(clock_font, text, finalPos, fontSize, spacing, WHITE);
        }else{
            DrawCircle(number_x, number_y, 1.0f, WHITE);
        }
    }
}

void draw_hour_hand(Vector2 position, int length, int time) {
    float angle = (float)time * (PI/21600) - PI/2;
    Vector2 end = { position.x + length * cosf(angle), position.y + length * sinf(angle) };
    DrawLineEx(position, end, 10.0f, BLACK);
}

void draw_minute_hand(Vector2 position, int length, int time) {
    float angle = (float)time * (PI/1800) - PI/2;
    Vector2 end = { position.x + length * cosf(angle), position.y + length * sinf(angle) };
    DrawLineEx(position, end, 5.0f, BLACK);
}

void draw_second_hand(Vector2 position, int length, int time) {
    float angle = (float)time * (PI/30) - PI/2;
    Vector2 end = { position.x + length * cosf(angle), position.y + length * sinf(angle) };
    DrawLineEx(position, end, 2.5f, RED);
}

int main() {

    int minutes_mid_x = SCREEN_WIDTH/2;
    int hours_mid_x = minutes_mid_x - 250;
    int seconds_mid_x = minutes_mid_x + 250;
    float digital_clock_y = (float)SCREEN_HEIGHT/6;

    float analog_clock_x = (float)SCREEN_WIDTH/2;
    float analog_clock_y = (float)4*SCREEN_HEIGHT/6;

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Clock");
    SetTargetFPS(60);

    clock_font = LoadFont("font.ttf");
    SetTextureFilter(clock_font.texture, TEXTURE_FILTER_BILINEAR);

    while (!WindowShouldClose())
    {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        // Extract individual digits using math
        int h_tens = t->tm_hour / 10;
        int h_ones = t->tm_hour % 10;

        int m_tens = t->tm_min / 10;
        int m_ones = t->tm_min % 10;

        int s_tens = t->tm_sec / 10;
        int s_ones = t->tm_sec % 10;

        BeginDrawing();
        ClearBackground(BLACK);

        // DIGITAL CLOCK
        // hours
        draw_full_number((Vector2){hours_mid_x - 55, digital_clock_y}, h_tens);
        draw_full_number((Vector2){hours_mid_x + 55, digital_clock_y}, h_ones);
        draw_separator((Vector2){hours_mid_x + 116, digital_clock_y});
        // minutes
        draw_full_number((Vector2){minutes_mid_x - 55, digital_clock_y}, m_tens);
        draw_full_number((Vector2){minutes_mid_x + 55, digital_clock_y}, m_ones);
        draw_separator((Vector2){minutes_mid_x + 116, digital_clock_y});
        // seconds
        draw_full_number((Vector2){seconds_mid_x - 55, digital_clock_y}, s_tens);
        draw_full_number((Vector2){seconds_mid_x + 55, digital_clock_y}, s_ones);

        // ANALOG CLOCK
        analog_face((Vector2){analog_clock_x, analog_clock_y}, clock_radius);
        // Clacualte Full Seconds
        int hour12 = t->tm_hour % 12;
        int full_seconds = hour12 * 3600 + t->tm_min * 60 + t->tm_sec;
        draw_hour_hand((Vector2){analog_clock_x, analog_clock_y}, clock_radius/2, full_seconds);
        draw_minute_hand((Vector2){analog_clock_x, analog_clock_y}, 3*clock_radius/4, full_seconds);
        draw_second_hand((Vector2){analog_clock_x, analog_clock_y}, clock_radius, full_seconds);
        DrawCircle(analog_clock_x, analog_clock_y, 10.0f, BLACK);

        EndDrawing();
    }

    UnloadFont(clock_font);
    CloseWindow();

    return 0;
}
