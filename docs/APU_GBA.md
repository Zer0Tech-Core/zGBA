# Documento Técnico de Referência: Subsistema de Áudio do Game Boy Advance (zGBA)

**Versão:** 1.0
**Propósito:** Especificação completa do subsistema de áudio do Game Boy Advance para a implementação do emulador zGBA. Este documento detalha a arquitetura de áudio, os canais DMG (Game Boy), os canais Direct Sound (PCM), os registradores de controle, a interface com DMA e temporizadores, e o pipeline de mixagem.

## 1. Introdução ao Subsistema de Áudio

O Game Boy Advance (GBA) possui um subsistema de áudio significativamente mais avançado que seus antecessores, com 6 canais de áudio independentes:

- **4 Canais DMG (Game Boy):** Canais 1, 2, 3 e 4, idênticos aos do Game Boy/Game Boy Color. Fornecem sons sintetizados (ondas quadradas, wave RAM e ruído).
- **2 Canais Direct Sound (PCM):** Canais A e B, que reproduzem amostras de áudio digital (Pulse-Code Modulation) de 8 bits armazenadas em buffers FIFO.

Todos os 6 canais são mixados por hardware e enviados para um conversor digital-analógico (DAC) de 8 bits. O DAC usa modulação por largura de pulso (PWM) para gerar o sinal de áudio final.

### 1.1. Características Principais

- **DAC:** 8 bits (resolução), suportando taxas de amostragem de até 65 kHz (tecnicamente, 262 kHz com resolução reduzida). O DAC é controlado pelo registrador `SOUNDBIAS`.
- **Mixagem:** Todos os canais são mixados automaticamente pelo hardware.
- **Controle de Volume:** Cada canal DMG tem controle de volume e pan (esquerda/direita). Os canais Direct Sound têm controle de volume e pan independentes.
- **FIFOs:** Os canais Direct Sound usam buffers FIFO de 16 bytes (4 amostras de 4 bytes) para armazenar os dados de áudio.
- **DMA e Temporizadores:** O áudio Direct Sound é normalmente alimentado por DMA, com a taxa de amostragem controlada por temporizadores (Timer 0 ou Timer 1). Isso permite reprodução de áudio contínua com mínimo uso da CPU.

## 2. Registradores de Áudio

A tabela abaixo descreve todos os registradores de áudio do GBA.

| Endereço | Nome | Tamanho | Descrição |
| :--- | :--- | :--- | :--- |
| `0x0400_0060` | `SOUND1CNT_L` | 16-bit | Canal 1: Controle de Sweep |
| `0x0400_0062` | `SOUND1CNT_H` | 16-bit | Canal 1: Comprimento, Duty Cycle e Envelope |
| `0x0400_0064` | `SOUND1CNT_X` | 16-bit | Canal 1: Frequência, Reset e Loop |
| `0x0400_0068` | `SOUND2CNT_L` | 16-bit | Canal 2: Comprimento, Duty Cycle e Envelope |
| `0x0400_006C` | `SOUND2CNT_H` | 16-bit | Canal 2: Frequência, Reset e Loop |
| `0x0400_0070` | `SOUND3CNT_L` | 16-bit | Canal 3: Enable e Banco da Wave RAM |
| `0x0400_0072` | `SOUND3CNT_H` | 16-bit | Canal 3: Comprimento e Volume |
| `0x0400_0074` | `SOUND3CNT_X` | 16-bit | Canal 3: Frequência, Reset e Loop |
| `0x0400_0078` | `SOUND4CNT_L` | 16-bit | Canal 4: Comprimento, Volume e Envelope |
| `0x0400_007C` | `SOUND4CNT_H` | 16-bit | Canal 4: Parâmetros de Ruído, Reset e Loop |
| `0x0400_0080` | `SOUNDCNT_L` | 16-bit | Volume Mestre e Pan dos Canais DMG |
| `0x0400_0082` | `SOUNDCNT_H` | 16-bit | Controle do Direct Sound e Mixagem |
| `0x0400_0084` | `SOUNDCNT_X` | 16-bit | Enable Mestre de Som e Status dos Canais DMG |
| `0x0400_0088` | `SOUNDBIAS` | 16-bit | Bias do DAC e Resolução PWM |
| `0x0400_0090` | `WAVE_RAM0_L` | 16-bit | Wave RAM Canal 3 (amostras 0-3) |
| `0x0400_0092` | `WAVE_RAM0_H` | 16-bit | Wave RAM Canal 3 (amostras 4-7) |
| `0x0400_0094` | `WAVE_RAM1_L` | 16-bit | Wave RAM Canal 3 (amostras 8-11) |
| `0x0400_0096` | `WAVE_RAM1_H` | 16-bit | Wave RAM Canal 3 (amostras 12-15) |
| `0x0400_0098` | `WAVE_RAM2_L` | 16-bit | Wave RAM Canal 3 (amostras 16-19) |
| `0x0400_009A` | `WAVE_RAM2_H` | 16-bit | Wave RAM Canal 3 (amostras 20-23) |
| `0x0400_009C` | `WAVE_RAM3_L` | 16-bit | Wave RAM Canal 3 (amostras 24-27) |
| `0x0400_009E` | `WAVE_RAM3_H` | 16-bit | Wave RAM Canal 3 (amostras 28-31) |
| `0x0400_00A0` | `FIFO_A_L` | 16-bit | FIFO Direct Sound A (amostras 0-1) |
| `0x0400_00A2` | `FIFO_A_H` | 16-bit | FIFO Direct Sound A (amostras 2-3) |
| `0x0400_00A4` | `FIFO_B_L` | 16-bit | FIFO Direct Sound B (amostras 0-1) |
| `0x0400_00A6` | `FIFO_B_H` | 16-bit | FIFO Direct Sound B (amostras 2-3) |

## 3. Canais DMG (Game Boy)

Os 4 canais DMG são idênticos aos do Game Boy. Eles são controlados por registradores específicos e geram sons sintetizados.

### 3.1. Canal 1: Onda Quadrada com Sweep

- **Descrição:** Gera uma onda quadrada com duty cycle variável, envelope (fade in/out) e sweep de frequência.
- **Registradores:** `SOUND1CNT_L`, `SOUND1CNT_H`, `SOUND1CNT_X`.
- **Parâmetros:**
    - **Sweep:** Controla a mudança de frequência ao longo do tempo (portamento).
    - **Duty Cycle:** 12.5%, 25%, 50%, 75%.
    - **Envelope:** Volume inicial, direção (aumentar/diminuir) e tempo de step.
    - **Frequência:** 64 Hz a 131 kHz.

### 3.2. Canal 2: Onda Quadrada

- **Descrição:** Gera uma onda quadrada com duty cycle variável e envelope. É idêntico ao Canal 1, mas sem a função de sweep.
- **Registradores:** `SOUND2CNT_L`, `SOUND2CNT_H`.

### 3.3. Canal 3: Wave RAM

- **Descrição:** Gera formas de onda arbitrárias usando uma tabela de 32 ou 64 amostras de 4 bits.
- **Registradores:** `SOUND3CNT_L`, `SOUND3CNT_H`, `SOUND3CNT_X`, e as 8 palavras de `WAVE_RAM`.
- **Características:**
    - **Wave RAM:** 64 amostras de 4 bits (256 bytes). Pode ser usada como um único banco de 64 amostras ou dois bancos de 32 amostras.
    - **Banking:** Os bancos podem ser comutados sem reiniciar o canal, permitindo a atualização dinâmica da wave RAM sem distorção.

### 3.4. Canal 4: Ruído

- **Descrição:** Gera ruído pseudo-aleatório usando um registrador de deslocamento com realimentação linear (LFSR).
- **Registradores:** `SOUND4CNT_L`, `SOUND4CNT_H`.
- **Parâmetros:**
    - **Modo:** 7 ou 15 estágios (ruído mais metálico ou mais branco).
    - **Frequência:** Controlada por um divisor de clock e um pré-divisor.

## 4. Canais Direct Sound (PCM)

Os canais Direct Sound A e B reproduzem amostras de áudio digital de 8 bits (valores signed, -128 a 127) armazenadas em FIFOs de 16 bytes.

### 4.1. Funcionamento

- **FIFOs:** Cada canal tem um FIFO de 16 bytes (4 amostras de 4 bytes). O software (ou DMA) escreve dados no FIFO. O hardware lê os dados do FIFO e os envia para o DAC.
- **Taxa de Amostragem:** Controlada por um temporizador (Timer 0 ou Timer 1). O FIFO consome uma amostra a cada overflow do temporizador.
- **Modos de Operação:**
    - **Modo DMA (Recomendado):** Um canal DMA (DMA1 para FIFO A, DMA2 para FIFO B) é configurado para preencher automaticamente o FIFO quando ele está vazio. Isso é feito configurando o modo de início do DMA para `11` (FIFO).
    - **Modo Interrupção:** Uma interrupção de temporizador é usada para preencher o FIFO manualmente.

### 4.2. Registradores de Controle

- **`SOUNDCNT_H`:** Controla o volume, pan (esquerda/direita), seleção do timer, e reset do FIFO para cada canal Direct Sound.
- **`SOUNDCNT_X`:** Bit 7 habilita/desabilita o circuito de som (economia de energia). Bits 0-3 são flags de status dos canais DMG (somente leitura).

### 4.3. Exemplo de Configuração (Modo DMA)

```c
// Configuração básica para tocar um som mono no canal A
REG_SOUNDCNT_H = 0x0604; // Volume 100%, Left/Right, FIFO reset
REG_SOUNDCNT_X = 0x80;   // Habilita som

// Configura o Timer 0 para a taxa de amostragem (ex: 18157 Hz)
REG_TM0D = 65536 - (16777216 / 18157); // Timer value

// Configura o DMA1 para preencher o FIFO A
REG_DMA1SAD = (u32)soundData;      // Endereço fonte
REG_DMA1DAD = (u32)&FIFO_A;        // Endereço destino (FIFO A)
REG_DMA1CNT_L = 0;                 // Tamanho do DMA (ignorado em modo FIFO)
REG_DMA1CNT_H = 0xB600;            // DMA ativo, modo FIFO, 32-bit, repetir

// Habilita o Timer 0
REG_TM0CNT = 0x80; // Habilita timer
```

## 5. Pipeline de Áudio

O pipeline de áudio do GBA funciona da seguinte forma:

1.  **Fonte dos Canais:** Cada canal gera seu áudio de acordo com seus registradores e dados.
    - **DMG:** Osciladores e wave RAM geram formas de onda.
    - **Direct Sound:** DMA ou interrupção preenchem os FIFOs com amostras PCM.
2.  **Mixagem:** O hardware soma os 6 canais em um sinal estéreo (esquerda/direita). Cada canal tem controle de volume e pan.
3.  **DAC:** O sinal mixado é convertido para analógico pelo DAC de 8 bits. O DAC usa modulação por largura de pulso (PWM). A resolução e a frequência são controladas por `SOUNDBIAS`.
4.  **Saída:** O sinal analógico é enviado para o alto-falante mono e para o fone de ouvido estéreo.

### 5.1. O Registrador `SOUNDBIAS`

- **Propósito:** Controla o bias (offset) do DAC e a resolução do PWM.
- **Bits:**
    - **Bits 0-9:** Valor de bias. Definido pela BIOS durante a inicialização. **Não deve ser alterado**.
    - **Bits 14-15:** Resolução do PWM.
        - `00`: 9-bit a 32768 Hz (padrão)
        - `01`: 8-bit a 65536 Hz (recomendado para a maioria dos jogos)
        - `10`: 7-bit a 131072 Hz
        - `11`: 6-bit a 262144 Hz

## 6. Buffering e Mixagem no Emulador

Para emular o áudio do GBA, você precisará implementar o seguinte:

1.  **Modelagem dos Canais:** Cada canal DMG e Direct Sound deve ser modelado como um objeto que mantém seu estado atual (frequência, envelope, posição na wave RAM, conteúdo do FIFO, etc.).
2.  **Atualização dos Canais:** A cada ciclo de áudio (ex: a cada amostra), cada canal deve ser atualizado:
    - **DMG:** Calcular o próximo valor de amostra com base na frequência, duty cycle, envelope, etc.
    - **Direct Sound:** Ler a próxima amostra do FIFO (se disponível).
3.  **Mixagem:** Somar as amostras de todos os canais (com seus respectivos volumes e pan) para gerar a amostra final esquerda e direita.
4.  **Saída:** Enviar a amostra mixada para o sistema de áudio do host (ex: usando uma biblioteca de áudio como SDL, OpenAL, etc.).
5.  **Sincronização:** A taxa de amostragem do emulador pode ser fixa (ex: 44.1 kHz ou 48 kHz). O emulador deve gerar amostras na taxa correta, amostrando os canais na frequência apropriada. O ideal é usar uma taxa de amostragem alta (ex: 44.1 kHz) e realizar o **upsampling** dos canais DMG e Direct Sound conforme necessário.

### 6.1. Considerações para o Emulador

- **Precisão:** A emulação precisa dos canais DMG (especialmente o Canal 4 - ruído) é crucial para a fidelidade do áudio. O LFSR deve ser modelado corretamente.
- **DMA e Temporizadores:** A emulação do DMA e dos temporizadores é fundamental para o Direct Sound. Quando o DMA preenche o FIFO, o emulador deve refletir isso no estado dos canais Direct Sound.
- **Taxa de Amostragem:** O DAC do GBA é de 8 bits. Ao mixar os canais, o resultado deve ser um valor de 16 bits (signed) para evitar clipping e perda de qualidade. A saída final pode ser convertida para 8 bits se desejado, mas a mixagem interna deve ser feita em maior resolução.
- **BIOS:** A BIOS do GBA contém funções de áudio (MusicPlayer) que muitos jogos usam. Emular a BIOS ou fornecer uma implementação alternativa (High-Level Emulation - HLE) é importante para a compatibilidade.

## 7. Conclusão

O subsistema de áudio do GBA é uma mistura da herança do Game Boy com recursos modernos de PCM. A compreensão detalhada de sua arquitetura, registradores e pipeline de mixagem é essencial para a implementação precisa e eficiente do emulador `zGBA`. Este documento serve como guia completo para essa tarefa.