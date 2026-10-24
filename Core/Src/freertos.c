/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Tetris Game on FreeRTOS (Variant 2)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "queue.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "fonts.h"
#include "kb.h"
#include "pca9538.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FIELD_W  10  // Ширина стакана (столбцов)
#define FIELD_H  20  // Высота стакана (строк)
#define CELL_SZ  3   // Размер клетки в пикселях (3x3)
#define OFFSET_X 4   // Отступ стакана слева
#define OFFSET_Y 2   // Отступ стакана сверху

// --- Словарь частот нот ---
#define N_B0  31
#define N_C1  33
#define N_CS1 35
#define N_D1  37
#define N_DS1 39
#define N_E1  41
#define N_F1  44
#define N_FS1 46
#define N_G1  49
#define N_GS1 52
#define N_A1  55
#define N_AS1 58
#define N_B1  62
#define N_C2  65
#define N_CS2 69
#define N_D2  73
#define N_DS2 78
#define N_E2  82
#define N_F2  87
#define N_FS2 93
#define N_G2  98
#define N_GS2 104
#define N_A2  110
#define N_AS2 117
#define N_B2  123
#define N_C3  131
#define N_CS3 139
#define N_D3  147
#define N_DS3 156
#define N_E3  165
#define N_F3  175
#define N_FS3 185
#define N_G3  196
#define N_GS3 208
#define N_A3  220
#define N_AS3 233
#define N_B3  247
#define N_C4  262
#define N_CS4 277
#define N_D4  294
#define N_DS4 311
#define N_E4  330
#define N_F4  349
#define N_FS4 370
#define N_G4  392
#define N_GS4 415
#define N_A4  440
#define N_AS4 466
#define N_B4  494
#define N_C5  523
#define N_CS5 554
#define N_D5  587
#define N_DS5 622
#define N_E5  659
#define N_F5  698
#define N_FS5 740
#define N_G5  784
#define N_GS5 831
#define N_A5  880
#define N_AS5 932
#define N_B5  988
#define N_C6  1047
#define N_CS6 1109
#define N_D6  1175
#define N_DS6 1245
#define N_E6  1319
#define N_F6  1397
#define N_FS6 1480
#define N_G6  1568
#define N_GS6 1661
#define N_A6  1760
#define N_AS6 1865
#define N_B6  1976
#define N_C7  2093
#define N_CS7 2217
#define N_D7  2349
#define N_DS7 2489
#define N_E7  2637
#define N_F7  2794
#define N_FS7 2960
#define N_G7  3136
#define N_GS7 3322
#define N_A7  3520
#define N_AS7 3729
#define N_B7  3951
#define N_C8  4186
#define N_CS8 4435
#define N_D8  4699
#define N_DS8 4978

/* Команды управления */
typedef enum {
    CMD_NONE = 0,
    CMD_LEFT,
    CMD_RIGHT,
    CMD_ROTATE,
    CMD_DROP,
    CMD_RESTART
} GameCmd_t;
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
QueueHandle_t xQueueInput = NULL;

uint8_t field[FIELD_H][FIELD_W];
uint32_t score = 0;
uint8_t game_over = 0;
int cur_x = 3, cur_y = 0;
int cur_type = 0, cur_rot = 0;

/* 7 фигур тетриса (I, J, L, O, S, T, Z) по 4 поворота */
const uint16_t TETROMINOES[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, // I
    {0x44C0, 0x8E00, 0x6440, 0x0E20}, // J
    {0x4460, 0x0E80, 0xC440, 0x2E00}, // L
    {0xCC00, 0xCC00, 0xCC00, 0xCC00}, // O
    {0x06C0, 0x8C40, 0x6C00, 0x4620}, // S
    {0x0E40, 0x4C40, 0x4E00, 0x4640}, // T
    {0x0C60, 0x4C80, 0xC600, 0x2640}  // Z
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void StartTaskInput(void *argument);
void StartTaskGame(void *argument);
void MX_FREERTOS_Init(void);

/* USER CODE BEGIN PFP */
void Show_Startup_Legend(void);
uint8_t GetPieceBit(int type, int rot, int px, int py);
uint8_t CheckCollision(int nx, int ny, int nrot);
void LockPiece(void);
void CheckLines(void);
void SpawnPiece(void);
void DrawCell(uint8_t cx, uint8_t cy, OLED_COLOR color);
void DrawGame(void);
void PlayDropSound(void);
void StartTaskAudio(void *argument);
/* USER CODE END PFP */

/**
  * @brief  Инициализация задач и очередей FreeRTOS
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* 1. Создаем очередь команд (размер 5 элементов) */
  xQueueInput = xQueueCreate(5, sizeof(GameCmd_t));

  /* 2. Задача опроса кнопок (Высокий приоритет) */
  xTaskCreate(StartTaskInput, "TaskInput", 256, NULL, tskIDLE_PRIORITY + 2, NULL);

  /* 3. Задача фоновой музыки (Низкий приоритет) */
  xTaskCreate(StartTaskAudio, "TaskAudio", 256, NULL, tskIDLE_PRIORITY, NULL);

  /* 4. Задача логики игры и отрисовки (Нормальный приоритет) */
  xTaskCreate(StartTaskGame, "TaskGame", 512, NULL, tskIDLE_PRIORITY + 1, NULL);
  /* USER CODE END Init */
}

/* USER CODE BEGIN Header_StartTaskInput */
/**
  * @brief  Задача опроса клавиатуры
  */
/* USER CODE END Header_StartTaskInput */
void StartTaskInput(void *argument)
{
  /* USER CODE BEGIN StartTaskInput */
  uint8_t rows[4] = {ROW1, ROW2, ROW3, ROW4};
  uint8_t was_pressed = 0;

  for(;;)
  {
    GameCmd_t cmd = CMD_NONE;

    for (int r = 0; r < 4; r++) {
        uint8_t k = Check_Row(rows[r]);
        if (k != 0) {
            if (!was_pressed) {
                was_pressed = 1;
                // Раскладка кнопок управления:
                if (r == 0 && k == 0x02) cmd = CMD_ROTATE;   // '2' (Вверх / Поворот)
                else if (r == 1 && k == 0x04) cmd = CMD_LEFT;    // '4' (Влево)
                else if (r == 1 && k == 0x01) cmd = CMD_RIGHT;   // '6' (Вправо)
                else if (r == 2 && k == 0x02) cmd = CMD_DROP;    // '8' (Вниз / Сброс)
                else if (r == 3 && k == 0x04) cmd = CMD_RESTART; // '*' (Рестарт)
            }
            break;
        } else if (r == 3) {
            was_pressed = 0;
        }
    }

    // Если есть действие — отправляем в очередь
    if (cmd != CMD_NONE && xQueueInput != NULL) {
        xQueueSend(xQueueInput, &cmd, 0);
    }

    vTaskDelay(pdMS_TO_TICKS(30)); // Спим 30 мс, отдавая процессор другим задачам
  }
  /* USER CODE END StartTaskInput */
}

/* USER CODE BEGIN Header_StartTaskGame */
/**
  * @brief  Задача логики Тетриса и обновления экрана
  */
/* USER CODE END Header_StartTaskGame */
void StartTaskGame(void *argument)
{
  /* USER CODE BEGIN StartTaskGame */
  oled_Init();
  Show_Startup_Legend();
  SpawnPiece();
  DrawGame();

  const TickType_t xGravityDelay = pdMS_TO_TICKS(400); // Скорость падения (400 мс)

  for(;;)
  {
    GameCmd_t received_cmd = CMD_NONE;

    // Ждем команду из очереди с таймаутом гравитации
    BaseType_t status = xQueueReceive(xQueueInput, &received_cmd, xGravityDelay);

    if (status == pdPASS) {
        // Получена команда игрока
        if (game_over && received_cmd == CMD_RESTART) {
            memset(field, 0, sizeof(field));
            score = 0;
            game_over = 0;
            SpawnPiece();
        } else if (!game_over) {
            if (received_cmd == CMD_LEFT && !CheckCollision(cur_x - 1, cur_y, cur_rot)) {
                cur_x--;
            } else if (received_cmd == CMD_RIGHT && !CheckCollision(cur_x + 1, cur_y, cur_rot)) {
                cur_x++;
            } else if (received_cmd == CMD_ROTATE) {
                int next_rot = (cur_rot + 1) % 4;
                if (!CheckCollision(cur_x, cur_y, next_rot)) cur_rot = next_rot;
            } else if (received_cmd == CMD_DROP) {
                if (!CheckCollision(cur_x, cur_y + 1, cur_rot)) cur_y++;
            }
        }
    } else {
        // Истек таймаут 400 мс -> Гравитация (шаг вниз)
        if (!game_over) {
            if (!CheckCollision(cur_x, cur_y + 1, cur_rot)) {
                cur_y++;
            } else {
                PlayDropSound();
                LockPiece();
                CheckLines();
                SpawnPiece();
            }
        }
    }

    DrawGame();
  }
  /* USER CODE END StartTaskGame */
}

/* ========================================================
   Вспомогательные функции игровой логики и графики
   ======================================================== */
/* USER CODE BEGIN 4 */
uint8_t GetPieceBit(int type, int rot, int px, int py) {
    uint16_t mask = TETROMINOES[type][rot % 4];
    int bit_idx = 15 - (py * 4 + px);
    return (mask & (1 << bit_idx)) ? 1 : 0;
}

uint8_t CheckCollision(int nx, int ny, int nrot) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (GetPieceBit(cur_type, nrot, x, y)) {
                int fx = nx + x;
                int fy = ny + y;
                if (fx < 0 || fx >= FIELD_W || fy >= FIELD_H) return 1;
                if (fy >= 0 && field[fy][fx]) return 1;
            }
        }
    }
    return 0;
}

void LockPiece(void) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (GetPieceBit(cur_type, cur_rot, x, y)) {
                int fx = cur_x + x;
                int fy = cur_y + y;
                if (fy >= 0 && fy < FIELD_H && fx >= 0 && fx < FIELD_W) {
                    field[fy][fx] = 1;
                }
            }
        }
    }
}

void CheckLines(void) {
    for (int y = FIELD_H - 1; y >= 0; y--) {
        int full = 1;
        for (int x = 0; x < FIELD_W; x++) {
            if (!field[y][x]) { full = 0; break; }
        }
        if (full) {
            score += 100;
            for (int ty = y; ty > 0; ty--) {
                for (int x = 0; x < FIELD_W; x++) {
                    field[ty][x] = field[ty - 1][x];
                }
            }
            for (int x = 0; x < FIELD_W; x++) field[0][x] = 0;
            y++;
        }
    }
}

void SpawnPiece(void) {
    cur_x = 3;
    cur_y = 0;
    cur_type = rand() % 7;
    cur_rot = 0;
    if (CheckCollision(cur_x, cur_y, cur_rot)) {
        game_over = 1;
    }
}

void DrawCell(uint8_t cx, uint8_t cy, OLED_COLOR color) {
    uint8_t px = OFFSET_X + cx * CELL_SZ;
    uint8_t py = OFFSET_Y + cy * CELL_SZ;
    for (uint8_t dy = 0; dy < 2; dy++) {
        for (uint8_t dx = 0; dx < 2; dx++) {
            oled_DrawPixel(px + dx, py + dy, color);
        }
    }
}

void DrawGame(void) {
    oled_Fill(Black);

    // Рамка стакана
    for (int y = 0; y <= FIELD_H * CELL_SZ + 1; y++) {
        oled_DrawPixel(OFFSET_X - 1, OFFSET_Y + y, White);
        oled_DrawPixel(OFFSET_X + FIELD_W * CELL_SZ, OFFSET_Y + y, White);
    }
    for (int x = 0; x <= FIELD_W * CELL_SZ; x++) {
        oled_DrawPixel(OFFSET_X + x, OFFSET_Y + FIELD_H * CELL_SZ + 1, White);
    }

    // Упавшие блоки
    for (int y = 0; y < FIELD_H; y++) {
        for (int x = 0; x < FIELD_W; x++) {
            if (field[y][x]) DrawCell(x, y, White);
        }
    }

    // Падающая фигура
    if (!game_over) {
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                if (GetPieceBit(cur_type, cur_rot, x, y)) {
                    DrawCell(cur_x + x, cur_y + y, White);
                }
            }
        }
    }

    // Текстовая панель справа
    char buf[16];
    oled_SetCursor(42, 2);
    oled_WriteString("TETRIS", Font_7x10, White);

    oled_SetCursor(42, 18);
    oled_WriteString("SCORE:", Font_7x10, White);
    oled_SetCursor(42, 30);
    snprintf(buf, sizeof(buf), "%lu", score);
    oled_WriteString(buf, Font_7x10, White);

    if (game_over) {
        oled_SetCursor(42, 45);
        oled_WriteString("GAME OVER", Font_7x10, White);
        oled_SetCursor(42, 55);
        oled_WriteString("[*] Retry", Font_7x10, White);
    } else {
        oled_SetCursor(42, 47);
        oled_WriteString("4:< 6:>", Font_7x10, White);
        oled_SetCursor(42, 56);
        oled_WriteString("2:^ 8:v", Font_7x10, White);
    }

    oled_UpdateScreen();
}

/**
  * @brief Заставка с раскладкой кнопок Тетриса при старте
  */
void Show_Startup_Legend(void) {
    oled_Fill(Black);

    // Заголовок
    oled_SetCursor(0, 0);
    oled_WriteString("TETRIS CONTROLS:", Font_7x10, White);

    // Сетка клавиш слева
    oled_SetCursor(0, 14);
    oled_WriteString("[1][2][3]", Font_7x10, White);
    oled_SetCursor(0, 26);
    oled_WriteString("[4][5][6]", Font_7x10, White);
    oled_SetCursor(0, 38);
    oled_WriteString("[7][8][9]", Font_7x10, White);
    oled_SetCursor(0, 50);
    oled_WriteString("[*][0][#]", Font_7x10, White);

    // Подсказка функций справа
    oled_SetCursor(70, 14);
    oled_WriteString("2:Rotate", Font_7x10, White);
    oled_SetCursor(70, 26);
    oled_WriteString("4:<  6:>", Font_7x10, White);
    oled_SetCursor(70, 38);
    oled_WriteString("8:Drop", Font_7x10, White);
    oled_SetCursor(70, 50);
    oled_WriteString("*:Retry", Font_7x10, White);

    oled_UpdateScreen();

    // Ждем 5000 мс (5 сек) ИЛИ нажатия любой кнопки через очередь FreeRTOS
    GameCmd_t dummy;
    xQueueReceive(xQueueInput, &dummy, pdMS_TO_TICKS(5000));
}

/* Dummy functions to fix FreeRTOS compilation errors */
__weak void configureTimerForRunTimeStats(void) {
}

__weak unsigned long getRunTimeCounterValue(void) {
    return 0;
}

extern TIM_HandleTypeDef htim2;

void PlayDropSound(void) {
    htim2.Instance->PSC = 83;      // Предделитель (частота шагов 1 МГц)
    htim2.Instance->ARR = 2000;    // Период (высота звука, ~500 Гц)
    htim2.Instance->CCR1 = 1000;   // Ширина импульса (громкость 50% от ARR)

    // Включаем звук
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

    // Ждем 50 миллисекунд (длительность "пика")
    vTaskDelay(pdMS_TO_TICKS(50));

    // Выключаем звук
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}

void StartTaskAudio(void *argument)
{
    // Укороченная мелодия (около 15 секунд)
    static const uint32_t zelda_melody[] = {
        N_AS4, 0, 0, N_AS4, N_AS4, N_AS4, N_AS4, N_AS4, 0, N_GS4, N_AS4, 0, 0, N_AS4, N_AS4, N_AS4, N_AS4, N_AS4, 0, N_GS4, N_AS4, 0, 0, N_AS4, N_AS4, N_AS4, N_AS4, N_AS4, N_F3, N_F3, N_F3, N_F3, N_F3, N_F3, N_F3, N_F3, N_F3, N_F3, N_AS4, N_F3, N_F3, 0, N_AS4, N_AS4, N_C5, N_D5, N_DS5, N_F5, 0, N_F5, N_F5, N_FS5, N_GS5, N_AS5, 0, N_AS5, N_AS5, N_AS5, N_GS5, N_FS5, N_GS5, 0, N_FS5, N_F5, N_F5, N_DS5, N_DS5, N_F5, N_FS5, N_F5, N_DS5, N_CS5, N_CS5, N_DS5, N_F5, N_DS5, N_CS5, N_C5, N_C5, N_D5, N_E5, N_G5, N_F5
    };
    static const uint32_t zelda_delays[] = {
        2, 8, 8, 8, 8, 8, 8, 6, 16, 16, 4, 8, 8, 8, 8, 8, 8, 6, 16, 16, 4, 8, 8, 8, 8, 8, 8, 8, 16, 16, 8, 16, 16, 8, 16, 16, 8, 8, 4, 4, 6, 16, 16, 16, 16, 16, 16, 2, 8, 8, 8, 8, 8, 2, 8, 8, 8, 8, 8, 8, 6, 16, 16, 2, 4, 8, 16, 16, 2, 8, 8, 8, 16, 16, 2, 8, 8, 8, 16, 16, 2, 4, 8, 16, 16, 8, 16, 16, 8, 16, 16, 8, 8, 4, 4, 6
    };

    int notes_count = sizeof(zelda_melody) / sizeof(uint32_t);

    for(;;)
    {
        for (int i = 0; i < notes_count; i++) {
            if (zelda_melody[i] == 0) {
                HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
            } else {
                htim2.Instance->PSC = 83;
                uint32_t arr_val = 1000000 / zelda_melody[i];

                htim2.Instance->ARR = arr_val;
                htim2.Instance->CCR1 = arr_val / 2;
                HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
            }

            vTaskDelay(pdMS_TO_TICKS(zelda_delays[i] * 20));
        }

        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
/* USER CODE END 4 */
