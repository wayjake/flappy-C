/*
 * ===============================================================================
 *
 *    _____ _                           ____  _         _
 *   |  ___| | __ _ _ __  _ __  _   _  | __ )(_)_ __ __| |
 *   | |_  | |/ _` | '_ \| '_ \| | | | |  _ \| | '__/ _` |
 *   |  _| | | (_| | |_) | |_) | |_| | | |_) | | | | (_| |
 *   |_|   |_|\__,_| .__/| .__/ \__, | |____/|_|_|  \__,_|
 *                 |_|   |_|    |___/
 *
 *   For Wokwi Emulator / Arduino with SSD1306 OLED Display
 *
 * ===============================================================================
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "images.h"

#define BUTTON_PIN      20
#define BUZZER_PIN      5
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* ============================================================================
 *   GAME CONSTANTS
 * ============================================================================
 *
 *   ┌────────────────────────────────────────────────────────────────────────┐
 *   │ SCORE                                                                  │
 *   │                    ┌──┐         ┌──┐         ┌──┐         ┌──┐        │
 *   │     ┌──┐           │  │         │  │         │  │         │  │        │
 *   │    <│@@│>  ════    └──┘   ════  └──┘   ════  └──┘   ════  └──┘        │
 *   │     └──┘           ┌──┐         ┌──┐         ┌──┐         ┌──┐        │
 *   │      ^             │  │         │  │         │  │         │  │        │
 *   │     bird           └──┘         └──┘         └──┘         └──┘        │
 *   └────────────────────────────────────────────────────────────────────────┘
 */

#define NUM_PIPES       4
#define PIPE_WIDTH      6
#define PIPE_SPACING    32
#define GAP_HEIGHT      30
#define GAP_MIN_Y       8
#define GAP_MAX_Y       32

#define BIRD_WIDTH      14
#define BIRD_HEIGHT     9
#define BIRD_START_X    30.0f
#define BIRD_START_Y    22.0f

#define GRAVITY         0.5f
#define FLAP_STRENGTH   1.5f
#define FLAP_DURATION   185
#define PIPE_SPEED      0.5f

#define FLAP_TONE_FREQ  1000
#define SCORE_TONE_FREQ 1200
#define TONE_DURATION   100
#define FLAP_SOUND_MS   40

/* ============================================================================
 *   MELODY DEFINITIONS
 * ============================================================================
 *   Note frequencies (Hz) for musical notes
 */
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_G5  784
#define NOTE_REST 0

// Startup melody: cheerful ascending jingle
const int startupMelody[] PROGMEM = {
    NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C5, NOTE_E5, NOTE_G5
};
const int startupDurations[] PROGMEM = {
    100, 100, 100, 100, 100, 200
};
#define STARTUP_MELODY_LEN 6

// Game over melody: descending sad tune
const int gameOverMelody[] PROGMEM = {
    NOTE_E5, NOTE_D5, NOTE_C5, NOTE_G4, NOTE_REST, NOTE_C4
};
const int gameOverDurations[] PROGMEM = {
    150, 150, 150, 200, 50, 300
};
#define GAMEOVER_MELODY_LEN 6

/* ============================================================================
 *   GAME STATE
 * ============================================================================ */

#define STATE_TITLE     0
#define STATE_PLAYING   1

int gameState = STATE_TITLE;
int score = 0;

float pipeX[NUM_PIPES];
int   gapY[NUM_PIPES];

float birdX = BIRD_START_X;
float birdY = BIRD_START_Y;
int   isFlapping = 0;

int  buttonWasPressed = 0;
unsigned long flapStartTime = 0;

int  playSoundFlag = 0;
unsigned long soundStartTime = 0;

/* ============================================================================
 *   XBM BITMAP HELPER
 * ============================================================================
 *   XBM format uses LSB-first bit ordering, but Adafruit's drawBitmap()
 *   expects MSB-first. This function handles the conversion.
 */
void drawXbm(int16_t x, int16_t y, int16_t width, int16_t height,
             const uint8_t *bitmap, uint16_t color) {
    int16_t byteWidth = (width + 7) / 8;
    for (int16_t j = 0; j < height; j++) {
        for (int16_t i = 0; i < width; i++) {
            uint8_t byteVal = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
            // XBM uses LSB-first, so check bit from right side
            if (byteVal & (1 << (i % 8))) {
                display.drawPixel(x + i, y + j, color);
            }
        }
    }
}

/* ============================================================================
 *   MELODY PLAYER
 * ============================================================================
 *   Plays a melody from PROGMEM arrays. Blocking call.
 */
void playMelody(const int *notes, const int *durations, int length) {
    for (int i = 0; i < length; i++) {
        int note = pgm_read_word(&notes[i]);
        int duration = pgm_read_word(&durations[i]);

        if (note == NOTE_REST) {
            noTone(BUZZER_PIN);
        } else {
            tone(BUZZER_PIN, note, duration);
        }
        delay(duration + 30);  // Gap between notes
    }
    noTone(BUZZER_PIN);
}

/* ============================================================================
 *   SETUP
 * ============================================================================ */
void setup() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Wire.begin(41, 42);  // SDA=41, SCL=42
    display.begin(0x3C, true);  // I2C address, reset
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);

    initializePipes();

    // Play startup jingle
    playMelody(startupMelody, startupDurations, STARTUP_MELODY_LEN);
}

/* ============================================================================
 *   PIPE INITIALIZATION
 * ============================================================================ */
void initializePipes() {
    for (int i = 0; i < NUM_PIPES; i++) {
        pipeX[i] = SCREEN_WIDTH + ((i + 1) * PIPE_SPACING);
        gapY[i] = random(GAP_MIN_Y, GAP_MAX_Y);
    }
}

/* ============================================================================
 *   MAIN GAME LOOP
 * ============================================================================ */
void loop() {
    display.clearDisplay();

    if (gameState == STATE_TITLE) {
        drawTitleScreen();
        handleTitleInput();
    } else if (gameState == STATE_PLAYING) {
        handleGameInput();
        updateBirdPhysics();
        updatePipes();
        checkCollisions();
        drawGameScreen();
        handleSound();
    }

    display.display();
}

/* ============================================================================
 *   TITLE SCREEN
 * ============================================================================ */
void drawTitleScreen() {
    drawXbm(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, background, SH110X_WHITE);
    drawXbm(20, 32, BIRD_WIDTH, BIRD_HEIGHT, birdSprite, SH110X_WHITE);

    display.setTextSize(2);
    display.setCursor(0, 4);
    display.print("Flappy");

    display.setTextSize(1);
    display.setCursor(0, 50);
    display.print("press to start");
}

void handleTitleInput() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        gameState = STATE_PLAYING;
    }
}

/* ============================================================================
 *   GAME INPUT HANDLING
 * ============================================================================ */
void handleGameInput() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (buttonWasPressed == 0) {
            flapStartTime = millis();
            isFlapping = 1;
            playSoundFlag = 1;
            buttonWasPressed = 1;
            soundStartTime = millis();
        }
    } else {
        buttonWasPressed = 0;
    }
}

/* ============================================================================
 *   BIRD PHYSICS
 * ============================================================================ */
void updateBirdPhysics() {
    if ((flapStartTime + FLAP_DURATION) < millis()) {
        isFlapping = 0;
    }

    if ((soundStartTime + FLAP_SOUND_MS) < millis()) {
        playSoundFlag = 0;
    }

    if (isFlapping == 0) {
        birdY = birdY + GRAVITY;
    } else {
        birdY = birdY - FLAP_STRENGTH;
    }
}

/* ============================================================================
 *   PIPE UPDATES
 * ============================================================================ */
void updatePipes() {
    for (int i = 0; i < NUM_PIPES; i++) {
        pipeX[i] = pipeX[i] - PIPE_SPEED;

        if (pipeX[i] < -PIPE_WIDTH - 1) {
            score = score + 1;
            tone(BUZZER_PIN, SCORE_TONE_FREQ, TONE_DURATION);
            gapY[i] = random(GAP_MIN_Y, GAP_MAX_Y);
            pipeX[i] = SCREEN_WIDTH;
        }
    }
}

/* ============================================================================
 *   COLLISION DETECTION
 * ============================================================================ */
void checkCollisions() {
    if (birdY > (SCREEN_HEIGHT - 1) || birdY < 0) {
        triggerGameOver();
        return;
    }

    for (int i = 0; i < NUM_PIPES; i++) {
        float birdRight = birdX + (BIRD_WIDTH / 2);
        if (pipeX[i] <= birdRight && birdRight <= pipeX[i] + PIPE_WIDTH) {
            float birdBottom = birdY + BIRD_HEIGHT;
            if (birdY < gapY[i] || birdBottom > gapY[i] + GAP_HEIGHT) {
                triggerGameOver();
                return;
            }
        }
    }
}

/* ============================================================================
 *   GAME OVER
 * ============================================================================ */
void triggerGameOver() {
    // Play game over melody
    playMelody(gameOverMelody, gameOverDurations, GAMEOVER_MELODY_LEN);

    gameState = STATE_TITLE;
    birdY = BIRD_START_Y;
    score = 0;

    initializePipes();
}

/* ============================================================================
 *   GAME SCREEN RENDERING
 * ============================================================================ */
void drawGameScreen() {
    // Draw score
    display.setTextSize(1);
    display.setCursor(3, 0);
    display.print(score);

    // Draw pipes
    for (int i = 0; i < NUM_PIPES; i++) {
        // Top pipe section
        display.fillRect((int)pipeX[i], 0, PIPE_WIDTH, gapY[i], SH110X_WHITE);
        // Bottom pipe section
        display.fillRect((int)pipeX[i], gapY[i] + GAP_HEIGHT, PIPE_WIDTH,
                         SCREEN_HEIGHT - gapY[i] - GAP_HEIGHT, SH110X_WHITE);
    }

    // Draw bird
    drawXbm((int)birdX, (int)birdY, BIRD_WIDTH, BIRD_HEIGHT, birdSprite, SH110X_WHITE);

    // Draw border
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
}

/* ============================================================================
 *   SOUND HANDLING
 * ============================================================================ */
void handleSound() {
    if (playSoundFlag == 1) {
        tone(BUZZER_PIN, FLAP_TONE_FREQ, TONE_DURATION);
    } else {
        noTone(BUZZER_PIN);
    }
}
