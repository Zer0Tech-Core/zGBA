# Documento Técnico de Referência: ARM7TDMI-S para o Game Boy Advance (zGBA)

**Versão:** 1.0
**Propósito:** Especificação completa da CPU ARM7TDMI-S (Rev 4) para a implementação do emulador zGBA. Este documento serve como a fonte definitiva de verdade para o comportamento, registradores, modos e exceções da CPU, fundamentado nos manuais ARM DDI 0234B e nas características do hardware do Game Boy Advance.

## 1. Introdução ao Processador ARM7TDMI-S

O coração do Game Boy Advance (GBA) é um processador System-on-Chip (SoC) chamado CPU AGB, que contém dois núcleos de processamento: um Sharp SM83 para compatibilidade retroativa com Game Boy e Game Boy Color, e o processador principal, o **ARM7TDMI-S** (ARM7 Thumb Debug Multiple Instruction-Synthesizable).

### 1.1. Filosofia de Design e Características Principais

O ARM7TDMI-S é um processador RISC (Reduced Instruction Set Computer) de 32 bits, implementando a arquitetura **ARMv4T**. Ele é otimizado para baixo consumo de energia, baixo custo e alta densidade de código, o que o tornou ideal para o GBA.

As características que definem este núcleo (representadas pelo sufixo TDMI-S) são:

- **`T` - Thumb:** Suporte ao conjunto de instruções Thumb de 16 bits, que oferece maior densidade de código, crucial para os barramentos de dados limitados do GBA.
- **`D` - Debug:** Extensões de hardware para depuração, utilizando a interface JTAG (Joint Test Action Group).
- **`M` - Enhanced Multiplier:** Um multiplicador aprimorado que executa multiplicações de 32 bits em poucos ciclos.
- **`I` - EmbeddedICE:** Uma macrocelular de depuração que permite breakpoints e watchpoints de hardware.
- **`S` - Synthesizable:** Uma versão sintetizável do núcleo, projetada para ser facilmente integrada em sistemas como o SoC do GBA.

### 1.2. O Pipeline de 3 Estágios

A CPU utiliza um pipeline de 3 estágios para aumentar o throughput de execução:

1.  **Fetch (Busca):** A instrução é buscada da memória.
2.  **Decode (Decodificação):** A instrução é decodificada e os operandos são lidos.
3.  **Execute (Execução):** A instrução é executada pela Unidade Lógica e Aritmética (ALU).

Enquanto uma instrução está sendo executada, a próxima é decodificada e a seguinte é buscada. O **Program Counter (PC)** aponta para a instrução que está sendo buscada, não para a que está sendo executada.

### 1.3. Jogos e o Conjunto de Instruções (ARM vs. Thumb)

Esta é uma consideração fundamental para o emulador, pois impacta a forma como você busca e decodifica as instruções.

O GBA pode executar instruções de 32 bits (**ARM**) e de 16 bits (**Thumb**). A decisão de qual conjunto usar é principalmente uma troca entre desempenho e densidade de código.

- **Modo ARM:**
    - Instruções de 32 bits.
    - Acesso total aos 16 registradores.
    - Execução condicional em todas as instruções.
    - **Uso:** Utilizado para código crítico de desempenho, rotinas de interrupção, ou quando se aproveita as instruções que não existem no Thumb (ex: multiplicação longa). No GBA, é eficiente quando executado a partir da memória IWRAM (barramento de 32 bits).

- **Modo Thumb:**
    - Instruções de 16 bits, um subconjunto das instruções ARM.
    - Acesso restrito aos registradores `r0-r7` (Low Registers) para a maioria das operações. Acesso limitado aos registradores `r8-r15` (High Registers) através de instruções `MOV`, `ADD`, `CMP`.
    - Sem execução condicional (exceto para a instrução `B` de branch). As condições são implementadas com branches.
    - **Uso:** A imensa maioria do código do GBA usa Thumb, pois a memória EWRAM e a ROM do cartucho são acessadas por um barramento de 16 bits. Isso dobra o throughput de busca de instruções, pois duas instruções Thumb podem ser buscadas por ciclo. As instruções Thumb também ocupam cerca de 65% do espaço das instruções ARM.

## 2. Modelo de Programação (Programmer's Model)

### 2.1. Estados de Operação

- **ARM State:** Executa instruções de 32 bits, alinhadas a palavras.
- **Thumb State:** Executa instruções de 16 bits, alinhadas a meias-palavras.

### 2.2. Conjunto de Registradores (Registers)

A CPU ARM7TDMI-S possui um total de 37 registradores de 32 bits: 31 registradores de propósito geral e 6 registradores de status.

#### 2.2.1. Registradores em Modo ARM

Em modo ARM, 16 registradores gerais (`r0` a `r15`) e um ou dois registradores de status (`CPSR` e `SPSR`) estão visíveis.

- **`r0` a `r12`:** Registradores de propósito geral.
- **`r13` (SP - Stack Pointer):** Aponta para o topo da pilha. Cada modo privilegiado tem seu próprio `r13` (ex: `r13_svc`).
- **`r14` (LR - Link Register):** Armazena o endereço de retorno de uma sub-rotina (instrução `BL`) ou o endereço para retornar de uma exceção.
- **`r15` (PC - Program Counter):** O contador de programa.

#### 2.2.2. Registradores em Modo Thumb

Em modo Thumb, apenas 8 registradores (`r0` a `r7`) são diretamente acessíveis para operações aritméticas e lógicas.

- **`r0` a `r7` (Low Registers):** São idênticos aos registradores ARM `r0-r7`.
- **`r8` a `r12` (High Registers):** Acessíveis apenas através das instruções especiais de movimento (`MOV`, `ADD` e `CMP`).
- **`r13` (SP):** Mapeia para o `r13` do modo ARM atual.
- **`r14` (LR):** Mapeia para o `r14` do modo ARM atual.
- **`r15` (PC):** Mapeia para o PC do modo ARM.
- **`CPSR` e `SPSR`:** São compartilhados com o modo ARM.

#### 2.2.3. Registradores de Status de Programa (PSRs)

A CPU possui um **Current Program Status Register (CPSR)** e cinco **Saved Program Status Registers (SPSRs)**, um para cada modo de exceção.

- **CPSR:**
    - **Bits [31:28] (N, Z, C, V):** Flags de condição. Definidos por operações aritméticas/lógicas.
    - **Bit [27] (Q):** Overflow de saturação (presente em ARM v5+, não usado no ARMv4T).
    - **Bits [26:8]:** Reservados (sempre zero).
    - **Bits [7:6] (I e F):** Bits de desabilitação de interrupção.
        - `I = 1`: Desabilita IRQs.
        - `F = 1`: Desabilita FIQs.
    - **Bit [5] (T):** Bit de estado Thumb.
        - `T = 0`: Modo ARM.
        - `T = 1`: Modo Thumb.
    - **Bits [4:0] (M[4:0]):** Bits de modo de operação. Definem o modo atual da CPU.

- **SPSR (para cada modo):** Salva o estado do CPSR quando uma exceção é tomada. Permite restaurar o estado anterior quando a rotina de exceção termina.

## 3. Tabela de Modos e Registradores

A tabela a seguir define o mapeamento de registradores para cada modo de operação, incluindo os registradores bancados. **Para o emulador, você precisará manter um array/estrutura separada para os registradores bancados de cada modo.**

| Modo | M[4:0] | Registradores Visíveis (ARM State) |
| :--- | :--- | :--- |
| **User (usr)** | `10000` | `r0` - `r14`, `r15`(PC), `CPSR` |
| **FIQ (fiq)** | `10001` | `r0` - `r7`, `r8_fiq` - `r14_fiq`, `r15`(PC), `CPSR`, `SPSR_fiq` |
| **IRQ (irq)** | `10010` | `r0` - `r12`, `r13_irq`, `r14_irq`, `r15`(PC), `CPSR`, `SPSR_irq` |
| **Supervisor (svc)** | `10011` | `r0` - `r12`, `r13_svc`, `r14_svc`, `r15`(PC), `CPSR`, `SPSR_svc` |
| **Abort (abt)** | `10111` | `r0` - `r12`, `r13_abt`, `r14_abt`, `r15`(PC), `CPSR`, `SPSR_abt` |
| **Undefined (und)** | `11011` | `r0` - `r12`, `r13_und`, `r14_und`, `r15`(PC), `CPSR`, `SPSR_und` |
| **System (sys)** | `11111` | `r0` - `r14`, `r15`(PC), `CPSR` |

**Notas de Implementação:**
- O modo **System** usa os mesmos registradores do modo User, mas é um modo privilegiado.
- O modo **FIQ** tem o maior número de registradores bancados (`r8-r14`), permitindo que rotinas FIQ não precisem salvar/restaurar registradores, tornando-as mais rápidas.

## 4. Exceções e Interrupções

Exceções interrompem o fluxo normal do programa. A CPU lida com elas em uma ordem de prioridade fixa.

### 4.1. Vetores de Exceção e Prioridade

| Prioridade | Endereço | Exceção | Modo de Entrada | Descrição |
| :--- | :--- | :--- | :--- | :--- |
| **1 (Mais Alta)** | `0x00000000` | **Reset** | Supervisor | Reinicia o processador. |
| **2** | `0x00000010` | **Data Abort** | Abort | Falha ao acessar dados (memória inválida/protegida). |
| **3** | `0x0000001C` | **FIQ** | FIQ | Interrupção de alta prioridade (Fast Interrupt Request). |
| **4** | `0x00000018` | **IRQ** | IRQ | Interrupção de baixa prioridade (Interrupt Request). |
| **5** | `0x0000000C` | **Prefetch Abort** | Abort | Falha ao buscar uma instrução. A exceção só é tomada se a instrução chegar ao estágio de execução. |
| **6** | `0x00000004` | **Undefined Instruction** | Undefined | Instrução inválida encontrada. |
| **7 (Mais Baixa)** | `0x00000008` | **SWI** | Supervisor | Instrução de software interrupt (chamada de sistema). |

### 4.2. Comportamento da CPU em uma Exceção

1.  **Salvar o estado:** O endereço de retorno é salvo no `LR` do modo de exceção (`r14_<modo>`). O valor depende da exceção.
2.  **Salvar o CPSR:** O CPSR atual é copiado para o `SPSR_<modo>`.
3.  **Mudar para o modo de exceção:** Os bits `M[4:0]` do CPSR são alterados para o modo da exceção.
4.  **Desabilitar interrupções:** O bit `I` (e `F` em FIQs) é definido no CPSR para evitar aninhamento.
5.  **Forçar a busca:** O PC é carregado com o endereço do vetor de exceção (veja tabela acima).

### 4.3. Comportamento da CPU ao Retornar de uma Exceção

O retorno de uma exceção é crítico e depende do tipo de exceção. Em geral, a CPU deve:

1.  **Restaurar o PC:** Mover o valor do `LR` (com um offset) para o PC. O offset varia para garantir o retorno correto.
2.  **Restaurar o CPSR:** Copiar o `SPSR_<modo>` de volta para o CPSR. Esta ação também restaura o modo e o estado Thumb/ARM.
3.  **Habilitar interrupções:** Se o SPSR tiver os bits `I` e `F` limpos, as interrupções serão reabilitadas.

**Tabela de Instruções de Retorno para o Emulador:**

| Exceção | Instrução de Retorno (ARM/Thumb) | Offsets (Nota) |
| :--- | :--- | :--- |
| **BL (Chamada de Sub-rotina)** | `MOV PC, R14` | `PC = LR` |
| **SWI** | `MOVS PC, R14_svc` | `PC = LR` (Volta para a próxima instrução) |
| **Undefined Instr.** | `MOVS PC, R14_und` | `PC = LR` (Volta para a próxima instrução) |
| **Prefetch Abort** | `SUBS PC, R14_abt, #4` | `PC = LR - 4` (Volta para a instrução que falhou) |
| **Data Abort** | `SUBS PC, R14_abt, #8` | `PC = LR - 8` (Volta para a instrução que falhou) |
| **FIQ** | `SUBS PC, R14_fiq, #4` | `PC = LR - 4` (Volta para a próxima instrução) |
| **IRQ** | `SUBS PC, R14_irq, #4` | `PC = LR - 4` (Volta para a próxima instrução) |
| **Reset** | N/A | N/A (CPU reinicia) |

**Nota para o Emulador:** A instrução de retorno, ao ser executada, deve disparar a lógica de restauração de estado. A instrução `MOVS` ou `SUBS` com destino PC indica que o `SPSR` deve ser restaurado para o `CPSR`.

## 5. Cálculo de Latência de Interrupção (Referência)

Para sistemas de tempo real, a latência máxima para FIQ e IRQ é importante. O manual ARM DDI 0234B fornece estas estimativas:

- **Latência Máxima FIQ (Zero Wait State):**
    1.  Sincronização (FIQ assíncrono): 2 ciclos de processador.
    2.  Instrução mais longa (LDM que carrega todos os registros, incluindo PC): 20 ciclos.
    3.  Entrada na exceção (Data Abort, se aplicável): 3 ciclos.
    4.  Entrada na exceção (FIQ): 2 ciclos.
    **Total:** **27 ciclos de processador**. (~0.675 µs a 40MHz).

- **Latência Mínima FIQ/IRQ (Síncrono):** **4 ciclos de processador.**

## 6. O Modo de Depuração (Debug) e o EmbeddedICE

O ARM7TDMI-S (Rev 4) inclui o **EmbeddedICE-RT**, que oferece dois modos de depuração: **Halt Mode** e **Monitor Mode**.

### 6.1. Modo Halt

- **Comportamento:** Ao encontrar um breakpoint ou watchpoint, a CPU para completamente e entra no estado de depuração (`DBGACK` = HIGH).
- **Registrador de Controle:** Bit 4 do Debug Control Register = `0`.
- **Ação:** O core para, permitindo que o depurador inspecione o estado.

### 6.2. Modo Monitor

- **Comportamento:** Ao encontrar um breakpoint ou watchpoint, a CPU não para. Em vez disso, ela gera uma exceção de Data Abort ou Prefetch Abort.
- **Registrador de Controle:** Bit 4 do Debug Control Register = `1`.
- **Ação:** Uma rotina de aborto (monitor) é executada, permitindo que a depuração aconteça "em tempo real" enquanto interrupções ainda são atendidas. Útil para sistemas de controle em tempo real.

## 7. Depuração no Contexto do GBA

Para o `zGBA`, a implementação do modo de depuração é complexa, mas crucial para testar jogos e desenvolver funcionalidades.

1.  **Breakpoints e Watchpoints:** O EmbeddedICE permite que o programador configure registradores de comparação (address, data, control). Quando uma condição é atendida, a CPU é interrompida (Halt Mode) ou uma exceção é gerada (Monitor Mode).

2.  **JTAG Interface e Scan Chains:** O depurador se comunica com o core através da interface JTAG (TDI, TDO, TMS, TCK). As **scan chains** são registradores de deslocamento que permitem ler/escrever dados do core.
    - **Scan Chain 1:** Usada para leitura/escrita de dados e para inserir instruções no pipeline.
    - **Scan Chain 2:** Usada para acessar os registradores do EmbeddedICE (breakpoints, watchpoints, DCC).

3.  **Debug Communications Channel (DCC):** É uma maneira de transferir dados entre o depurador (host) e a CPU alvo.
    - **Registradores DCC:**
        - **DCC Control Register (Address 0x0):** Usado para handshaking. Os bits 0 (R - Read) e 1 (W - Write) indicam se o buffer está cheio ou vazio.
        - **DCC Data Register (Address 0x1):** Onde os dados são escritos/lidos.
    - **Funcionamento (do ponto de vista do depurador):**
        - **Escrever para a CPU:** Verifica se o bit R está `0`. Se sim, escreve no Data Register. A CPU lê com uma instrução `MRC` e o bit R é limpo.
        - **Ler da CPU:** A CPU escreve com uma instrução `MCR`. O bit W é setado. O depurador lê o Data Register e o bit W é limpo.

## 8. O Mapeamento de Memória do GBA (Relevante para a CPU)

A CPU ARM7TDMI-S no GBA pode endereçar 4GB de espaço, mas o hardware mapeia apenas algumas regiões. As mais críticas para o seu emulador são:

- **`0x00000000 - 0x00003FFF` (16 KB): BIOS (Boot ROM).**
    - **Ação:** A CPU sempre começa a executar aqui após o Reset. O boot ROM contém a rotina de inicialização, splash screen, verificação de cartucho e várias funções de sistema (math, decompressão, sound, etc.).

- **`0x02000000 - 0x0203FFFF` (256 KB): EWRAM (External WRAM).**
    - **Ação:** Memória RAM externa de 16 bits. É onde a maioria dos jogos armazena dados e executa código Thumb (devido ao barramento de 16 bits). É significativamente mais lenta que a IWRAM.

- **`0x03000000 - 0x03007FFF` (32 KB): IWRAM (Internal WRAM).**
    - **Ação:** Memória RAM interna de 32 bits. É a memória mais rápida. Ideal para código ARM crítico e para a pilha (stack).

- **`0x04000000 - 0x040003FF` (1 KB): I/O Registers.**
    - **Ação:** Esta região é a porta de comunicação com o hardware. Lendo ou escrevendo aqui, o programa controla o display, o som, a entrada, os temporizadores, DMA, etc.

- **`0x05000000 - 0x050003FF` (1 KB): Palette RAM.**
    - **Ação:** Armazena as paletas de cores para o PPU.

- **`0x06000000 - 0x06017FFF` (96 KB): VRAM.**
    - **Ação:** A memória de vídeo, onde ficam os tiles, mapas de tela e dados de sprites.

- **`0x07000000 - 0x070003FF` (1 KB): OAM.**
    - **Ação:** Object Attribute Memory. Armazena as propriedades dos sprites (posição, tamanho, etc.).

- **`0x08000000 - 0x09FFFFFF` (até 32 MB): Game Pak ROM (Wait State 0).**
- **`0x0A000000 - 0x0BFFFFFF` (até 32 MB): Game Pak ROM (Wait State 1).**
- **`0x0C000000 - 0x0DFFFFFF` (até 32 MB): Game Pak ROM (Wait State 2).**
    - **Ação:** Região onde a ROM do cartucho é mapeada. A CPU pode acessá-la diretamente para executar instruções ou ler dados. O mapeamento em três regiões permite diferentes configurações de tempo de espera (wait states). A maioria dos jogos usa a região `0x08000000`.

## 9. O Sistema de Boot e a BIOS

O comportamento da CPU após o Reset é fundamental:

1.  **Reset:** O sinal `nRESET` é ativado e depois desativado.
2.  **Entrada em Modo Supervisor:** A CPU entra no modo Supervisor (M[4:0] = `10011`).
3.  **Desabilitação de Interrupções:** Os bits `I` e `F` do CPSR são setados (desabilitam IRQ e FIQ).
4.  **Modo ARM:** O bit `T` do CPSR é limpo, colocando a CPU em modo ARM (instruções de 32 bits).
5.  **PC = `0x00000000`:** A CPU começa a executar a partir do endereço da BIOS.

A BIOS então:
1.  Verifica o hardware, inicializa o display com o splash screen da Nintendo.
2.  Verifica a existência e o tipo do cartucho.
3.  Realiza uma verificação de soma de verificação (checksum) na ROM do cartucho.
4.  Se o cartucho for Game Boy, ativa o modo de compatibilidade (desabilita o ARM7, liga o Sharp SM83).
5.  Se for um cartucho GBA válido, pula para o endereço `0x08000000` (início da ROM do cartucho) para começar a execução do jogo.

---

Este documento fornece uma base técnica sólida para iniciar o desenvolvimento do emulador `zGBA`. A compreensão profunda deste conteúdo é crucial para a precisão e estabilidade do seu projeto.