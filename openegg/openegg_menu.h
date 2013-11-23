
#include "openegg_ui.h"

#define  MENU_NAV  0
#define  MENU_MASK 0x3fff
#define  MENU_END  0x8000
#define  MENU_ITER 0x4000

/* flags */
#define DISP_TEXT     0x0001
#define DISP_DIGITS   0x0002
#define CMD_STICKY    0x0004
#define ITER_START    0x0010
#define ITER_NEXT     0x0020
#define ITER_BACK     0x0040 
#define ITER_ACK      0x0080
#define ITER_NACK     0x0100
#define ITER_IDLE     0x0200

#define SET_ITER_NACK(x) (x) &= ~(ITER_NEXT|ITER_BACK|ITER_START|ITER_ACK); (x) |= ITER_NACK;
#define SET_ITER_ACK(x) (x) &= ~(ITER_NEXT|ITER_BACK|ITER_START|ITER_NACK); (x) |= ITER_ACK;
         

typedef struct _mstate_t const * pmstate_t;

typedef struct _mstate_t {
        const pmstate_t next;
        const int id;
        const char *text;
} mstate_t;

/* The menu has to be defined by yourself */
extern const mstate_t menu[];

/* State processing callback */
typedef void (*openegg_menu_callback)(int id, int *flags, char *text, char *digits);

void openegg_menu_state(openegg_menu_callback cb);
void openegg_menu_init(void);
