#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#define WIDTH 64
#define HEIGHT 32
#define SCALE 15

#define W_WIDTH WIDTH * SCALE
#define W_HEIGHT HEIGHT * SCALE

typedef struct chip8 {
  unsigned short opcode;
  unsigned short pc;
  unsigned short I;

  unsigned char V[16];
  unsigned char memory[4096];

  unsigned char delay_timer;
  unsigned char sound_timer;

  unsigned short stack[16];
  unsigned short sp;

  unsigned char key[16];

  int drawFlag;
  unsigned char screen[64 * 32];
} c8_impl;

uint32_t graphics[WIDTH * HEIGHT];

unsigned char fontset[80] =
{
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

int keymap[0x10] = {
  SDLK_1,
  SDLK_2,
  SDLK_3,
  SDLK_4,
  SDLK_q,
  SDLK_w,
  SDLK_e,
  SDLK_r,
  SDLK_a,
  SDLK_s,
  SDLK_d,
  SDLK_f,
  SDLK_z,
  SDLK_x,
  SDLK_c,
  SDLK_v
};

c8_impl c8;

void initialize() {

  c8.pc = 0x200;
  c8.opcode = 0;
  c8.I = 0;
  c8.sp = 0;

  // Load fontset into first 80 bytes of memory
  memcpy(c8.memory, fontset, 80);

  // Init buffers
  memset(c8.screen, 0, WIDTH * HEIGHT * sizeof(unsigned char));
  memset(c8.key, 0, 0x10);
  memset(graphics, 0, WIDTH * HEIGHT * sizeof(uint32_t));
}

void emulateCycle() {

  // Fetch
  c8.opcode = c8.memory[c8.pc] << 8 | c8.memory[c8.pc + 1];

  printf("opcode: 0x%X\n", c8.opcode);

  // Decode
  switch(c8.opcode & 0xF000) {
    case 0x0000:
      switch(c8.opcode & 0x000F) {
        case 0x0000: // 0x00E0: Clears the screen
          memset(c8.screen, 0, WIDTH * HEIGHT);
          c8.pc += 2;
          break;

        case 0x000E: // 0x00EE: Returns from subroutine
          // Execute opcode
          c8.sp--;
          c8.pc = c8.stack[c8.sp];
          c8.pc += 2;
          break;

        default:
          printf("Unknown c8.opcode [0x0000]: 0x%X\n", c8.opcode);
          break;

      }
      break;
    case 0x1000: // 0x1NNN: jumps to address NNN
      c8.pc = c8.opcode & 0x0FFF;
      break;

    case 0x2000: // 0x2NNN: calls subroutine at NNN
      c8.stack[c8.sp] = c8.pc;
      c8.sp++;
      c8.pc = c8.opcode & 0x0FFF;
      break;

    case 0x3000: // 3XNN: skips the next instruction if VX equals NN.
      if (c8.V[(c8.opcode & 0x0F00) >> 8] == (c8.opcode & 0x00FF))
        c8.pc += 4;
      else
        c8.pc += 2;
      break;

    case 0x4000: // 4xNN: skips the next instruction if VX doesn't equal NN.
      if (c8.V[(c8.opcode & 0x0F00) >> 8] != (c8.opcode & 0x00FF))
        c8.pc += 4;
      else
        c8.pc += 2;
      break;

    case 0x5000: // 0x5XY0: skips the next instruction if VX equals VY.
      if (c8.V[(c8.opcode & 0x0F00) >> 8] == c8.V[(c8.opcode & 0x00F0) >> 4])
        c8.pc += 4;
      else
        c8.pc += 2;
      break;

    case 0x6000: // 0x6XNN: sets VX to NN.
      c8.V[(c8.opcode & 0x0F00) >> 8] = (c8.opcode & 0x00FF);
      c8.pc += 2;
      break;

    case 0x7000: // 7XNN 	Adds NN to VX.
      c8.V[(c8.opcode & 0x0F00) >> 8] += (c8.opcode & 0x00FF);
      c8.pc += 2;
      break;

    case 0x8000:
      switch(c8.opcode & 0x000F) {
        case 0x0000: // 8XY0 	Sets VX to the value of VY.
          c8.V[(c8.opcode & 0x0F00) >> 8] = c8.V[(c8.opcode & 0x00F0) >> 4];
          c8.pc += 2;
          break;

        case 0x0001: // 8XY1 	Sets VX to VX or VY.
          c8.V[(c8.opcode & 0x0F00) >> 8] |= c8.V[(c8.opcode & 0x00F0) >> 4];
          c8.pc += 2;
          break;

        case 0x0002: // 8XY2 	Sets VX to VX and VY.
          c8.V[(c8.opcode & 0x0F00) >> 8] &= c8.V[(c8.opcode & 0x00F0) >> 4];
          c8.pc += 2;
          break;

        case 0x0003: // 8XY3 	Sets VX to VX xor VY.
          c8.V[(c8.opcode & 0x0F00) >> 8] ^= c8.V[(c8.opcode & 0x00F0) >> 4];
          c8.pc += 2;
          break;

        case 0x0004: // 8XY4 Adds VY to VX. VF is set to 1 when there's a carry,
                     // and to 0 when there isn't.
          if(c8.V[(c8.opcode & 0x00F0) >> 4] > (0xFF - c8.V[(c8.opcode & 0x0F00) >> 8]))
            c8.V[0xF] = 1; // carry
          else
            c8.V[0xF] = 0;
          c8.V[(c8.opcode & 0x0F00) >> 8] += c8.V[(c8.opcode & 0x00F0) >> 4];
          c8.pc += 2;
          break;

        case 0x0005: // 8XY5 VY is subtracted from VX. VF is set to 0 when
                     // there's a borrow, and 1 when there isn't.
          if(c8.V[(c8.opcode & 0x0F00) >> 8] > c8.V[(c8.opcode & 0x00F0) >> 4])
            c8.V[0xF] = 1; // carry
          else
            c8.V[0xF] = 0;
          c8.V[(c8.opcode & 0x0F00) >> 8] -= c8.V[(c8.opcode & 0x00F0) >> 4];

          if (c8.V[(c8.opcode & 0x0F00) >> 8] < 0)
            c8.V[(c8.opcode & 0x0F00) >> 8] += 256;
          c8.pc += 2;
          break;

        case 0x0006: // 8XY6 Shifts VX right by one. VF is set to the value of
                     // the least significant bit of VX before the shift.
          c8.V[0xF] = c8.V[(c8.opcode & 0x0F00) >> 8] & 1;
          c8.V[(c8.opcode & 0x0F00) >> 8] = c8.V[(c8.opcode & 0x0F00) >> 8] >> 1;
          c8.pc += 2;
          break;

        case 0x0007: // 8XY7 Sets VX to VY minus VX. VF is set to 0 when there's
                     // a borrow, and 1 when there isn't.
          if(c8.V[(c8.opcode & 0x0F00) >> 8] < c8.V[(c8.opcode & 0x00F0) >> 4])
            c8.V[0xF] = 1;
          else
            c8.V[0xF] = 0;
          c8.V[(c8.opcode & 0x0F00) >> 8] = c8.V[(c8.opcode & 0x00F0) >> 4] - c8.V[(c8.opcode & 0x0F00) >> 8];
          c8.pc += 2;
          break;

        case 0x000E: // 8XYE Shifts VX left by one. VF is set to the value of
                     // the most significant bit of VX before the shift.
          c8.V[0xF] = (c8.V[(c8.opcode & 0x0F00) >> 8] & 0xFF) >> 7; // value of msb of VX
          c8.V[(c8.opcode & 0x0F00) >> 8] = c8.V[(c8.opcode & 0x0F00) >> 8] << 1;
          c8.pc += 2;
          break;

        default:
          printf("Unknown c8.opcode [0x8000]: 0x%X\n", c8.opcode);
          break;
      }
      break;

    case 0x9000: // 9XY0 	Skips the next instruction if VX doesn't equal VY.
      if (c8.V[(c8.opcode & 0x0F00) >> 8] != c8.V[(c8.opcode & 0x00F0) >> 4])
        c8.pc += 4;
      else
        c8.pc += 2;
      break;

    case 0xA000: // ANNN: Sets I to the address NNN
      c8.I = c8.opcode & 0x0FFF;
      c8.pc += 2;
      break;

    case 0xB000: // BNNN Jumps to the address NNN plus V0.
      c8.pc = (c8.opcode & 0x0FFF) + c8.V[0x0];
      break;

    case 0xC000: // CXNN Sets VX to the result of a bitwise and operation on a random number and NN.
      c8.V[(c8.opcode & 0x0F00) >> 8] = rand() & (c8.opcode & 0x00FF);
      c8.pc += 2;
      break;

    case 0xD000:
    {
    /* Sprites stored in memory at location in index register (I), 8bits wide.
     * Wraps around the screen. If when drawn, clears a pixel, register VF is
     * set to 1 otherwise it is zero. All drawing is XOR drawing (i.e. it
     * toggles the screen pixels). Sprites are drawn starting at position VX,
     * VY. N is the number of 8bit rows that need to be drawn. If N is greater
     * than 1, second line continues at position VX, VY+1, and so on.
     */
      unsigned short x = c8.V[(c8.opcode & 0x0F00) >> 8];
      unsigned short y = c8.V[(c8.opcode & 0x00F0) >> 4];
      unsigned short height = c8.opcode & 0x000F;
      unsigned short pixel;

      c8.V[0xF] = 0;
      for (int yline = 0; yline < height; yline++) {
        pixel = c8.memory[c8.I + yline];
        for(int xline = 0; xline < 8; xline++) {
          if((pixel & (0x80 >> xline)) != 0) {
            if(c8.screen[(x + xline + ((y + yline) * 64))] == 1)
              c8.V[0xF] = 1;
            c8.screen[x + xline + ((y + yline) * 64)] ^= 1;
          }
        }
      }
      c8.drawFlag = 1;
      c8.pc += 2;
    }
    break;

    case 0xE000:
      switch(c8.opcode & 0x000F) {
        case 0x000E: // EX9E 	Skips the next instruction if the key stored in VX is pressed.
          if(c8.key[c8.V[(c8.opcode & 0x0F00) >> 8]] != 0)
            c8.pc += 4;
          else
            c8.pc += 2;
          break;

        case 0x0001: // EXA1 	Skips the next instruction if the key stored in VX isn't pressed.
          if(c8.key[c8.V[(c8.opcode & 0x0F00) >> 8]] == 0)
            c8.pc += 4;
          else
            c8.pc += 2;
          break;

        default:
          printf("Unknown c8.opcode [0xE000]: 0x%X\n", c8.opcode);
          break;
      }
      break;

    case 0xF000:
      switch(c8.opcode & 0x00FF) {
        case 0x0007: // FX07 	Sets VX to the value of the delay timer.
          c8.V[(c8.opcode & 0x0F00) >> 8] = c8.delay_timer;
          c8.pc += 2;
          break;

        case 0x000A: // FX0A 	A key press is awaited, and then stored in VX.
          {
          const uint8_t* key_state = SDL_GetKeyboardState(NULL);

          for (int i = 0; i < 0x10; i++) {
            if (key_state[SDL_GetScancodeFromKey(keymap[i])]) {
              c8.V[(c8.opcode & 0x0F00) >> 8] = i;
              c8.pc += 2;
            }
          }
          break;
          }

        case 0x0015: // FX15 	Sets the delay timer to VX.
          c8.delay_timer = c8.V[(c8.opcode & 0x0F00) >> 8];
          c8.pc += 2;
          break;

        case 0x0018: // FX18 	Sets the sound timer to VX.
          c8.sound_timer = c8.V[(c8.opcode & 0x0F00) >> 8];
          c8.pc += 2;
          break;

        case 0x001E: // FX1E 	Adds VX to I.
          c8.I += c8.V[(c8.opcode & 0x0F00) >> 8];
          c8.pc += 2;
          break;

        case 0x0029:
          /* FX29 Sets I to the _location_ of the sprite for the
           * character in VX. Characters 0-F (in hexadecimal) are
           * represented by a 4x5 font.
           */
           c8.I = c8.V[(c8.opcode & 0x0F00) >> 8] * 5;
           c8.pc += 2;
           break;

        case 0x0033:
          /* FX33 Stores the Binary-coded decimal representation of VX, with the most
           * significant of three digits at the address in I, the middle digit at
           * I plus 1, and the least significant digit at I plus 2. (In other words,
           * take the decimal representation of VX, place the hundreds digit in
           * memory at location in I, the tens digit at location I+1, and the ones
           * digit at location I+2.)
           */
           c8.memory[c8.I] = c8.V[(c8.opcode & 0x0F00) >> 8] / 100;
           c8.memory[c8.I + 1] = (c8.V[(c8.opcode & 0x0F00) >> 8] / 10) % 10;
           c8.memory[c8.I + 2] = (c8.V[(c8.opcode & 0x0F00) >> 8] % 100) % 10;
           c8.pc += 2;
           break;

        case 0x0055: // FX55 	Stores V0 to VX in memory starting at address I.
          memcpy(c8.memory + c8.I, c8.V, sizeof(unsigned char) * ((c8.opcode & 0x0F00) >> 8) + 1);
          c8.I += ((c8.opcode & 0x0F00) >> 8) + 0x001;
          c8.pc += 2;
          break;

        case 0x0065: // FX65 	Fills V0 to VX with values from memory starting at address I.
          memcpy(c8.V, c8.memory + c8.I, sizeof(unsigned char) * ((c8.opcode & 0x0F00) >> 8) + 1);
          c8.I += ((c8.opcode & 0x0F00) >> 8) + 0x001;
          c8.pc += 2;
          break;

        default:
          printf("Unknown opcode [0xF000]: 0x%X\n", c8.opcode);
          break;
      }
      break;

    default:
      printf("Unknown opcode: 0x%X\n", c8.opcode);
      break;
  }

  // Decrement timers
  if(c8.delay_timer > 0)
    c8.delay_timer--;

  if(c8.sound_timer > 0) {
    if(c8.sound_timer == 1)
      // Sound would play
    c8.sound_timer--;
  }
}

void loadGame(const char* name) {

  size_t io_buf_size = 4096 - 512;

  char io_buf[io_buf_size];
  FILE *file;

  file = fopen(name, "rb");  // r for read, b for binary
  if (!file) {
    printf("Error: could not open file %s\n", name);
    exit(1);
  }

  fread(io_buf, sizeof(io_buf), io_buf_size, file);

  memcpy(c8.memory + 512, io_buf, io_buf_size);

}

void update_texture(SDL_Renderer* renderer, SDL_Texture* texture) {

  int x, y;
  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
      if (c8.screen[x + (y * WIDTH)] == 0)
        graphics[x + (y * WIDTH)] = 0;
      else
        graphics[x + (y * WIDTH)] = -1;

  // Update screen with new pixel buffer
  if (SDL_UpdateTexture(texture, NULL, graphics, WIDTH * sizeof(uint32_t)) < 0) {
    printf("Error updating texture");
    exit(1);
  }

  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);

  SDL_Delay(15);
}

int main(int argc, char **argv) {

  initialize();

  loadGame(argv[1]);

  if ((SDL_Init(SDL_INIT_EVERYTHING) < 0)) {
      printf("Could not initialize SDL: %s.\n", SDL_GetError());
      exit(-1);
  }

  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  SDL_Texture *texture = NULL;

  window = SDL_CreateWindow("CHIP-8 Emulator",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            0, 0,
                            SDL_WINDOW_OPENGL);

  renderer = SDL_CreateRenderer(window, -1, 0);

  texture = SDL_CreateTexture(renderer,
                               SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING,
                               WIDTH,
                               HEIGHT);

  SDL_SetWindowSize(window, W_WIDTH, W_HEIGHT);

  int quit = 0;
  while (!quit) {

      SDL_PumpEvents();

      const uint8_t* key_state = SDL_GetKeyboardState(NULL);

      if (key_state[SDL_GetScancodeFromKey(SDLK_ESCAPE)])
        quit = 1;

      for (int i = 0; i < 0x10; i++) {
        if (key_state[SDL_GetScancodeFromKey(keymap[i])])
          c8.key[i] = 1;
      }

      emulateCycle();

      if (c8.drawFlag) {
          update_texture(renderer, texture);
          c8.drawFlag = 0;
      }

      memset(c8.key, 0, 0x10);
  }

  SDL_Quit();
  return 0;
}
