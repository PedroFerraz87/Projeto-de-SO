/*
  vm_manager_fifo.c
  Simulador simples de Gerenciamento de Memória Virtual com:
   - Tabela de páginas
   - Memória física com N frames
   - Disco (simulado por arquivo) para "swap out"
   - Política de substituição FIFO
   - Conta de page faults e swaps

  Como usar (ex.: compilar/rodar):
    gcc vm_manager_fifo.c -o vm_manager_fifo
    ./vm_manager_fifo

  O programa pedirá:
    - número de frames na memória física
    - número de páginas no espaço virtual
    - comprimento da sequência de referências
    - sequência de páginas referenciadas (números entre 0 e num_pages-1)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int valid;      // bit de validade: está em memória física?
    int frame_no;   // se valid==1, número do frame; caso contrário -1
    int dirty;      // (não usado para escrita, mas mantido para extensão)
} PageTableEntry;

typedef struct FrameNode {
    int frame_no;
    int page;                // página atualmente carregada no frame, -1 se livre
    struct FrameNode *next;  // para fila FIFO
} FrameNode;

/* Disco simulado: gravamos linha "page X swapped out" num arquivo quando swap ocorre */
const char *SWAP_FILE = "swap_simulated.txt";

/* FIFO queue helpers */
typedef struct {
    FrameNode *head;
    FrameNode *tail;
    int size;
} FIFOQueue;

void fifo_init(FIFOQueue *q) {
    q->head = q->tail = NULL;
    q->size = 0;
}

void fifo_push(FIFOQueue *q, int frame_no, int page) {
    FrameNode *n = (FrameNode*)malloc(sizeof(FrameNode));
    n->frame_no = frame_no;
    n->page = page;
    n->next = NULL;
    if (!q->head) q->head = n;
    else q->tail->next = n;
    q->tail = n;
    q->size++;
}

FrameNode* fifo_pop(FIFOQueue *q) {
    if (!q->head) return NULL;
    FrameNode *n = q->head;
    q->head = n->next;
    if (!q->head) q->tail = NULL;
    n->next = NULL;
    q->size--;
    return n;
}

void fifo_remove_page(FIFOQueue *q, int page) {
    FrameNode *prev = NULL;
    FrameNode *cur = q->head;
    while (cur) {
        if (cur->page == page) {
            if (!prev) q->head = cur->next;
            else prev->next = cur->next;
            if (cur == q->tail) q->tail = prev;
            free(cur);
            q->size--;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

/* Simulação */
int main() {
    int num_frames, num_pages, ref_len;
    printf("=== Simulador de Memoria Virtual (FIFO) ===\n\n");
    printf("Digite o número de frames (memória física): ");
    if (scanf("%d", &num_frames) != 1) return 1;
    printf("Digite o número de páginas no espaço virtual: ");
    if (scanf("%d", &num_pages) != 1) return 1;
    if (num_frames <= 0 || num_pages <= 0) {
        printf("Valores devem ser positivos.\n");
        return 1;
    }

    printf("Digite o comprimento da sequência de referências: ");
    if (scanf("%d", &ref_len) != 1) return 1;
    if (ref_len <= 0) {
        printf("Comprimento deve ser positivo.\n");
        return 1;
    }

    int *refs = malloc(sizeof(int) * ref_len);
    printf("Digite a sequência de páginas (valores entre 0 e %d) separadas por espaço ou enter:\n", num_pages - 1);
    for (int i = 0; i < ref_len; ++i) {
        if (scanf("%d", &refs[i]) != 1) {
            printf("Erro de leitura.\n");
            free(refs);
            return 1;
        }
        if (refs[i] < 0 || refs[i] >= num_pages) {
            printf("Página inválida %d (deve estar em [0,%d]).\n", refs[i], num_pages - 1);
            free(refs);
            return 1;
        }
    }

    /* inicializa tabela de páginas */
    PageTableEntry *pt = malloc(sizeof(PageTableEntry) * num_pages);
    for (int i = 0; i < num_pages; ++i) {
        pt[i].valid = 0;
        pt[i].frame_no = -1;
        pt[i].dirty = 0;
    }

    /* inicializa frames */
    int *frame_to_page = malloc(sizeof(int) * num_frames);
    for (int i = 0; i < num_frames; ++i) frame_to_page[i] = -1;

    FIFOQueue fifo;
    fifo_init(&fifo);

    int free_frames = num_frames;
    int next_free_frame = 0; // para alocar frames inicialmente
    long page_faults = 0;
    long swaps_out = 0;

    /* limpa/abre arquivo de swap simulado */
    FILE *swapf = fopen(SWAP_FILE, "w");
    if (swapf) {
        fprintf(swapf, "=== Swap simulated log ===\n");
        fclose(swapf);
    }

    printf("\n--- Iniciando simulação ---\n");
    for (int step = 0; step < ref_len; ++step) {
        int page = refs[step];
        printf("Referência %2d: página %d --> ", step + 1, page);

        if (pt[page].valid) {
            printf("HIT (está no frame %d)\n", pt[page].frame_no);
            // nothing else for FIFO on hit
        } else {
            /* page fault */
            page_faults++;
            printf("PAGE FAULT -> ");

            if (free_frames > 0) {
                /* há frames livres: carregar página sem desalocar */
                int frame = next_free_frame++;
                frame_to_page[frame] = page;
                pt[page].valid = 1;
                pt[page].frame_no = frame;
                fifo_push(&fifo, frame, page);
                free_frames--;
                printf("carregado no frame %d (frames livres agora %d)\n", frame, free_frames);
            } else {
                /* precisão: implementar FIFO - desalocar a página mais antiga (head) */
                FrameNode *victim = fifo_pop(&fifo);
                if (!victim) {
                    fprintf(stderr, "Erro interno: fila FIFO vazia mesmo sem frames livres.\n");
                    free(refs); free(pt); free(frame_to_page);
                    return 1;
                }
                int victim_frame = victim->frame_no;
                int victim_page = victim->page;

                /* "escrever no disco" (simulado) */
                swapf = fopen(SWAP_FILE, "a");
                if (swapf) {
                    fprintf(swapf, "Step %d: swapped out page %d from frame %d\n", step+1, victim_page, victim_frame);
                    fclose(swapf);
                }
                swaps_out++;

                /* atualiza tabela de páginas do victim (torná-la inválida) */
                pt[victim_page].valid = 0;
                pt[victim_page].frame_no = -1;

                /* carrega a nova página no frame do vítima */
                frame_to_page[victim_frame] = page;
                pt[page].valid = 1;
                pt[page].frame_no = victim_frame;

                /* coloca novo par no final da fila FIFO */
                fifo_push(&fifo, victim_frame, page);

                printf("desalocado page %d (frame %d) -> carregado page %d no mesmo frame\n", victim_page, victim_frame, page);

                free(victim);
            }
        }
    }

    /* resultado */
    printf("\n--- Estatísticas ---\n");
    printf("Número de referências: %d\n", ref_len);
    printf("Page faults: %ld\n", page_faults);
    printf("Swaps (simulados) para disco: %ld (log em '%s')\n", swaps_out, SWAP_FILE);
    printf("Estado final dos frames (frame_no: page):\n");
    for (int f = 0; f < num_frames; ++f) {
        printf("  frame %2d: %d\n", f, frame_to_page[f]);
    }

    /* cleanup */
    free(refs);
    free(pt);

    /* limpar fila */
    FrameNode *cur = fifo.head;
    while (cur) {
        FrameNode *nx = cur->next;
        free(cur);
        cur = nx;
    }
    free(frame_to_page);

    printf("\nSimulação finalizada.\n");
    return 0;
}
