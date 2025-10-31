# Projeto-de-SO

## Gerenciador de Memória Virtual - Simulador FIFO (C)

Implementação em C de um simulador de gerenciamento de memória virtual com:
- Tabela de páginas
- Política de substituição FIFO (First-In-First-Out)
- Simulação de swap para disco (arquivo `swap_simulated.txt`)
- Contagem de page faults e swaps

### Compilar
```bash
gcc vm_manager_fifo.c -o vm_manager_fifo

./vm_manager_fifo

