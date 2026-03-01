#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_WORDS 22

typedef struct {
    const char *dragon;
    const char *meaning;
    int learned;   // 0 = not learned, 1 = learned
} WordOfPower;

/* apocrypha!!! */
static WordOfPower apocrypha[MAX_WORDS] = {
    {"FUS", "Force", 0},
    {"RO",  "Balance", 0},
    {"DAH", "Push", 0},

    {"YOL",  "Fire", 0},
    {"TOOR", "Inferno", 0},
    {"FO",   "Frost", 0},
    {"KRAH", "Cold", 0},

    {"ZUL",  "Voice", 0},
    {"TIID", "Time", 0},

    {"WULD", "Whirlwind", 0},
    {"NAH",  "Fury", 0},
    {"KEST", "Tempest", 0},
    {"LAAS", "Life", 0},
    {"YAH",  "Seek", 0},
    {"NIR",  "Hunt", 0},
    {"KREN", "Banish", 0},
    {"DAAN", "Doom", 0},
    {"JOOR", "Mortal", 0},
    {"ZAH",  "Finite", 0},
    {"FRII", "Freeze", 0},
    {"SU",   "Air", 0},
    {"GRAH", "Battle", 0}
};

static int word_count = MAX_WORDS;

/* seeking shout index among the pages of the black book */
int seek_word(const char *word) {
    for (int i = 0; i < word_count; i++) {
        if (strcmp(apocrypha[i].dragon, word) == 0) {
            return i;
        }
    }
    return -1;
}

/* shout a word */
void shout_word(const char *word) {
    int idx = seek_word(word);

    if (idx == -1) {
        printf("Hermaeus Mora: Such a sound carries no meaning in my endless library.\n");
        return;
    }

    if (!apocrypha[idx].learned) {
        printf("Hermaeus Mora: The meaning of your shout eludes you.\n");
    } else {
        printf("%s — %s\n", apocrypha[idx].dragon, apocrypha[idx].meaning);
    }
}

/* Learn a word */
void learn_word(const char *word) {
    int idx = seek_word(word);

    if (idx == -1) {
        printf("Hermaeus Mora: Such a sound carries no meaning in my endless library.\n");
        return;
    }

    if (apocrypha[idx].learned) {
        printf("Hermaeus Mora: You already possess this knowledge.\n");
        return;
    }

    apocrypha[idx].learned = 1;
    printf("%s — %s\n", apocrypha[idx].dragon,apocrypha[idx].meaning);
}

/* List known words */
void list_known(void) {
    int any = 0;

    for (int i = 0; i < word_count; i++) {
        if (apocrypha[i].learned) {
            printf("%s — %s\n", apocrypha[i].dragon,apocrypha[i].meaning);
            any = 1;
        }
    }

    if (!any) {
        printf("Hermaeus Mora: You have learned nothing... yet.\n");
    }
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage:\n");
        printf("  thuum WORD\n");
        printf("  thuum --learn WORD\n");
        printf("  thuum --known\n");
        return 0;
    }

    if (strcmp(argv[1], "--learn") == 0) {
        if (argc < 3) {
            printf("Usage: thuum --learn WORD\n");
            return 0;
        }
        learn_word(argv[2]);
        return 0;
    }

    if (strcmp(argv[1], "--known") == 0) {
        list_known();
        return 0;
    }

    shout_word(argv[1]);

    return 0;
}

