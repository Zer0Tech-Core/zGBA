# Documento Técnico de Referência: Sistema de Memória do Game Boy Advance (zGBA)

**Versão:** 1.0
**Propósito:** Especificação completa do sistema de memória do Game Boy Advance para a implementação do emulador zGBA. Este documento detalha o mapa de memória, os tipos de barramento, os tempos de espera (wait states), os periféricos mapeados em memória e as considerações críticas para a emulação precisa do hardware.

## 1. Introdução ao Sistema de Memória

O Game Boy Advance (GBA) utiliza um sistema de memória de 32 bits com uma arquitetura mista de 16 e 32 bits. O processador ARM7TDMI, com seu barramento de endereços de 32 bits, pode endereçar teoricamente 4 GB de espaço, mas o hardware do GBA mapeia apenas regiões específicas. O design do sistema de memória é uma combinação de otimização de custos, consumo de energia e desempenho, o que resultou em uma hierarquia de memória com diferentes velocidades e larguras de barramento.

### 1.1. A Hierarquia de Memória e a Influência do Thumb

A decisão de design mais impactante no sistema de memória do GBA foi a inclusão do conjunto de instruções Thumb. Como a maioria das memórias (EWRAM e ROM) usa um barramento de dados de 16 bits, o uso de instruções Thumb de 16 bits permite que a CPU busque duas instruções por ciclo, dobrando o throughput de busca em comparação com o modo ARM (que busca uma instrução de 32 bits a cada dois ciclos). Isso torna o Thumb o modo de operação padrão e mais eficiente para a grande maioria dos jogos.

## 2. Mapa de Memória Completo

A tabela a seguir apresenta o mapa de memória completo do GBA, conforme visto pela CPU, com detalhes sobre largura de barramento, wait states e propósito.

| Endereço Inicial | Endereço Final | Tamanho | Nome | Largura | Wait State | Propósito |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `0x0000_0000` | `0x0000_3FFF` | 16 KB | **BIOS** | 32-bit | 0 | ROM de inicialização. Executável mas não legível. Contém a rotina de boot e funções de sistema (SWIs). |
| `0x0200_0000` | `0x0203_FFFF` | 256 KB | **EWRAM** | 16-bit | 1 | RAM externa. Principal área para dados e código (especialmente Thumb). Mais lenta que IWRAM. |
| `0x0300_0000` | `0x0300_7FFF` | 32 KB | **IWRAM** | 32-bit | 0 | RAM interna. A mais rápida. Ideal para código ARM crítico e para a pilha (stack). |
| `0x0400_0000` | `0x0400_03FF` | 1 KB | **I/O Registers** | 32-bit | 0 | Registradores de hardware para controlar o display, som, DMA, temporizadores, etc. |
| `0x0500_0000` | `0x0500_03FF` | 1 KB | **Palette RAM** | 16-bit | 0 | Memória de paletas de cores para backgrounds e sprites. |
| `0x0600_0000` | `0x0601_7FFF` | 96 KB | **VRAM** | 16-bit | 0 | Memória de vídeo. Armazena tiles, mapas de tela, dados de sprites e framebuffers. |
| `0x0700_0000` | `0x0700_03FF` | 1 KB | **OAM** | 32-bit | 0 | Object Attribute Memory. Armazena os atributos dos sprites (posição, tamanho, etc.). |
| `0x0800_0000` | `0x09FF_FFFF` | 32 MB | **Game Pak ROM (WS0)** | 16-bit | 0 | **Região primária** para ROM do cartucho. A maioria dos jogos executa a partir daqui. |
| `0x0A00_0000` | `0x0BFF_FFFF` | 32 MB | **Game Pak ROM (WS1)** | 16-bit | 1 | Mirror da ROM com wait state 1 (mais lento). Usado para cartuchos com ROMs mais lentas. |
| `0x0C00_0000` | `0x0DFF_FFFF` | 32 MB | **Game Pak ROM (WS2)** | 16-bit | 2 | Mirror da ROM com wait state 2 (mais lento ainda). |
| `0x0E00_0000` | `0x0E00_FFFF` | 64 KB | **Cart RAM (SRAM/Flash)** | 8-bit | 3 | Memória do cartucho para saves. Acessível apenas como 8-bit. |
| `0x0F00_0000` | `0x0F00_FFFF` | 64 KB | **Mirror de Cart RAM** | 8-bit | 3 | Mirror da região de Cart RAM. |
| `0x1000_0000` | `0x1FFF_FFFF` | - | **Região Reservada** | - | - | Acessos a esta região são inválidos. O comportamento é imprevisível e pode ler instruções pré-buscadas. |

### 2.1. Notas sobre o Mapa de Memória

- **BIOS (0x00000000):** É uma ROM de 16 KB localizada no chip CPU AGB. Ela é executável, mas **não é legível** diretamente pela CPU. Tentativas de leitura nesta região resultam em um comportamento especial: a CPU retorna o valor da instrução atualmente pré-buscada (a instrução após a que está sendo usada para ler a memória). Isso é uma medida de proteção e anti-pirataria.
- **EWRAM (0x02000000) vs. IWRAM (0x03000000):** A distinção entre estas duas regiões é crucial para o desempenho. EWRAM é de 16 bits e mais lenta, enquanto IWRAM é de 32 bits e extremamente rápida. Os jogos geralmente colocam código ARM crítico e a pilha em IWRAM, enquanto o código Thumb e dados grandes vão para EWRAM.
- **Game Pak ROM (0x08000000 - 0x0DFFFFFF):** A ROM do cartucho pode ser mapeada em três regiões de 32 MB (`0x08`, `0x0A`, `0x0C`), cada uma com uma configuração diferente de wait state (WS). A região `0x08` é a padrão e mais rápida (WS0). As outras regiões são usadas para cartuchos com ROMs mais lentas ou para acessar dados que não precisam ser tão rápidos.
- **Prefetch Buffer:** Para mitigar a falta de cache, o GBA possui um **Prefetch Buffer** na interface do cartucho. Este buffer de 8 palavras de 16 bits armazena endereços sequenciais quando a CPU não está acessando o cartucho. No entanto, como a CPU busca instruções continuamente, o buffer tem um impacto limitado.

## 3. Wait States (Tempos de Espera)

Wait states são ciclos de espera adicionados a um acesso à memória para acomodar dispositivos mais lentos. A tabela abaixo detalha os wait states padrão e configuráveis para cada região.

| Região | Largura | Wait State | Comportamento no Emulador |
| :--- | :--- | :--- | :--- |
| **BIOS** | 32-bit | 0 (Não configurável) | 1 ciclo por acesso de 32 bits. |
| **IWRAM** | 32-bit | 0 (Não configurável) | 1 ciclo por acesso de 32 bits. Acesso a 8/16 bits também é rápido. |
| **I/O Regs** | 32-bit | 0 (Não configurável) | 1 ciclo por acesso. |
| **Palette RAM** | 16-bit | 0 | 1 ciclo por acesso de 16 bits. Acesso a 32 bits é 2 ciclos. |
| **VRAM** | 16-bit | 0 | 1 ciclo por acesso de 16 bits. Acesso a 32 bits é 2 ciclos. |
| **OAM** | 32-bit | 0 | 1 ciclo por acesso de 32 bits. |
| **EWRAM** | 16-bit | 1 | 2 ciclos por acesso de 16 bits. Acesso a 32 bits leva 2 acessos de 16 bits (4 ciclos). |
| **Game Pak ROM (WS0)** | 16-bit | Configurável | **WS0 padrão:** 1 ciclo para acesso inicial + 1 ciclo para acesso subsequente. O wait state inicial é configurável via `WAITCNT`. |
| **Cart RAM (SRAM/Flash)** | 8-bit | Configurável | **WS padrão:** 4 ciclos. Configurável via `WAITCNT`. |

### 3.1. O Registrador WAITCNT (`0x04000204`)

O registrador `WAITCNT` permite que o software configure os wait states para a ROM do cartucho e a SRAM. A tabela de bits é a seguinte:

- **Bits 0-1 (SRAM):** Wait state para SRAM.
    - `00`: 4 ciclos (padrão)
    - `01`: 3 ciclos
    - `10`: 2 ciclos
    - `11`: 8 ciclos
- **Bits 2-3 (WS0 Initial):** Wait state inicial para ROM em `0x08000000`.
    - `00`: 4 ciclos (padrão)
    - `01`: 3 ciclos
    - `10`: 2 ciclos
    - `11`: 8 ciclos
- **Bit 4 (WS0 Subsequent):** Wait state subsequente para ROM em `0x08000000`.
    - `0`: 2 ciclos (padrão)
    - `1`: 1 ciclo
- **Bits 5-6 (WS1 Initial):** Wait state inicial para ROM em `0x0A000000`.
    - `00`: 4 ciclos, `01`: 3 ciclos, `10`: 2 ciclos, `11`: 8 ciclos
- **Bit 7 (WS1 Subsequent):**
    - `0`: 4 ciclos, `1`: 1 ciclo
- **Bits 8-9 (WS2 Initial):** Wait state inicial para ROM em `0x0C000000`.
    - `00`: 4 ciclos, `01`: 3 ciclos, `10`: 2 ciclos, `11`: 8 ciclos
- **Bit A (WS2 Subsequent):**
    - `0`: 8 ciclos, `1`: 1 ciclo
- **Bit E (Prefetch):** Habilita/desabilita o Prefetch Buffer.

## 4. Periféricos Mapeados em Memória (I/O Registers)

A região de I/O (`0x04000000 - 0x040003FF`) é a porta de comunicação com o hardware do GBA. Cada registrador de 16 ou 32 bits controla uma funcionalidade específica. Abaixo está um resumo dos registradores mais importantes, com base no `gbadoc`.

### 4.1. Registradores de Display (Graphics)

| Endereço | Nome | Descrição |
| :--- | :--- | :--- |
| `0x04000000` | `DISPCNT` | Registrador de controle do display (modo de vídeo, ativação de camadas, etc.). |
| `0x04000004` | `DISPSTAT` | Status do display (VBlank, HBlank, scanline). |
| `0x04000006` | `VCOUNT` | Número da linha de scan atual. |
| `0x04000008` | `BG0CNT` | Controle do Background 0. |
| `0x0400000A` | `BG1CNT` | Controle do Background 1. |
| `0x0400000C` | `BG2CNT` | Controle do Background 2. |
| `0x0400000E` | `BG3CNT` | Controle do Background 3. |
| `0x04000010` | `BG0HOFS` | Scroll horizontal do BG0. |
| `0x04000012` | `BG0VOFS` | Scroll vertical do BG0. |
| `0x04000014` | `BG1HOFS` | Scroll horizontal do BG1. |
| `0x04000016` | `BG1VOFS` | Scroll vertical do BG1. |
| `0x04000018` | `BG2HOFS` | Scroll horizontal do BG2. |
| `0x0400001A` | `BG2VOFS` | Scroll vertical do BG2. |
| `0x0400001C` | `BG3HOFS` | Scroll horizontal do BG3. |
| `0x0400001E` | `BG3VOFS` | Scroll vertical do BG3. |
| `0x04000020` | `BG2PA` | Parâmetros Affine do BG2 (escala X). |
| `0x04000022` | `BG2PB` | Parâmetros Affine do BG2 (shear X). |
| `0x04000024` | `BG2PC` | Parâmetros Affine do BG2 (shear Y). |
| `0x04000026` | `BG2PD` | Parâmetros Affine do BG2 (escala Y). |
| `0x04000028` | `BG2X` | Posição X do BG2 (Affine). |
| `0x0400002C` | `BG2Y` | Posição Y do BG2 (Affine). |
| `0x04000030` | `BG3PA` | Parâmetros Affine do BG3. |
| `0x04000040` | `WIN0H` | Coordenadas X da Janela 0. |
| `0x04000042` | `WIN1H` | Coordenadas X da Janela 1. |
| `0x04000044` | `WIN0V` | Coordenadas Y da Janela 0. |
| `0x04000046` | `WIN1V` | Coordenadas Y da Janela 1. |
| `0x04000048` | `WININ` | Configuração de exibição dentro das janelas 0 e 1. |
| `0x0400004A` | `WINOUT` | Configuração de exibição fora das janelas e dentro da janela de sprites. |
| `0x0400004C` | `MOSAIC` | Controle do efeito Mosaico. |
| `0x04000050` | `BLDCNT` | Configuração de blending (modo e camadas alvo/fonte). |
| `0x04000052` | `BLDALPHA` | Coeficientes de blend (A e B). |
| `0x04000054` | `BLDY` | Valor de luminância para efeitos de fade. |

### 4.2. Registradores de Áudio (Sound)

| Endereço | Nome | Descrição |
| :--- | :--- | :--- |
| `0x04000060` | `NR10` | Controle de sweep do Canal 1. |
| `0x04000062` | `NR11` | Controle de duty cycle e envelope do Canal 1. |
| `0x04000064` | `NR12` | Controle de volume inicial e envelope do Canal 1. |
| `0x04000066` | `NR13` | Frequência do Canal 1 (Low Byte). |
| `0x04000067` | `NR14` | Frequência do Canal 1 (High Byte) e controle de reset. |
| `0x04000068` | `NR21` | Controle de duty cycle e envelope do Canal 2. |
| `0x04000069` | `NR22` | Controle de volume inicial e envelope do Canal 2. |
| `0x0400006A` | `NR23` | Frequência do Canal 2 (Low Byte). |
| `0x0400006B` | `NR24` | Frequência do Canal 2 (High Byte) e controle de reset. |
| `0x0400006C` | `NR30` | Controle de enable e banco do Canal 3. |
| `0x0400006D` | `NR31` | Controle de comprimento do Canal 3. |
| `0x0400006E` | `NR32` | Controle de volume do Canal 3. |
| `0x0400006F` | `NR33` | Frequência do Canal 3 (Low Byte). |
| `0x04000070` | `NR34` | Frequência do Canal 3 (High Byte) e controle de reset. |
| `0x04000071` | `NR41` | Controle de comprimento do Canal 4. |
| `0x04000072` | `NR42` | Controle de volume inicial e envelope do Canal 4. |
| `0x04000073` | `NR43` | Controle de frequência e polinômio do Canal 4 (ruído). |
| `0x04000074` | `NR44` | Reset do Canal 4. |
| `0x04000080` | `NR50` | Controle de volume mestre e pan DMG. |
| `0x04000082` | `NR51` | Controle de pan dos canais DMG. |
| `0x04000084` | `NR52` | Enable mestre de som e status dos canais DMG. |
| `0x04000088` | `SOUNDBIAS` | Controle de bias PWM. |
| `0x04000090` | `WAVE_RAM0_L` | Wave RAM do Canal 3 (amostras 0-3). |
| `0x040000A0` | `FIFO_A` | FIFO do Direct Sound A (canal de áudio PCM). |
| `0x040000A4` | `FIFO_B` | FIFO do Direct Sound B (canal de áudio PCM). |

### 4.3. Registradores de DMA

| Endereço | Nome | Descrição |
| :--- | :--- | :--- |
| `0x040000B0` | `DMA0SAD` | Endereço fonte do DMA0. |
| `0x040000B4` | `DMA0DAD` | Endereço destino do DMA0. |
| `0x040000B8` | `DMA0CNT_L` | Contagem de transferência do DMA0. |
| `0x040000BA` | `DMA0CNT_H` | Controle do DMA0. |
| `0x040000BC` | `DMA1SAD` | Endereço fonte do DMA1. |
| ... | ... | ... |
| `0x040000D4` | `DMA3SAD` | Endereço fonte do DMA3. |
| `0x040000D8` | `DMA3DAD` | Endereço destino do DMA3. |
| `0x040000DC` | `DMA3CNT_L` | Contagem de transferência do DMA3. |
| `0x040000DE` | `DMA3CNT_H` | Controle do DMA3. |

### 4.4. Registradores de Timer

| Endereço | Nome | Descrição |
| :--- | :--- | :--- |
| `0x04000100` | `TM0D` | Valor do Timer 0. |
| `0x04000102` | `TM0CNT` | Controle do Timer 0. |
| `0x04000104` | `TM1D` | Valor do Timer 1. |
| `0x04000106` | `TM1CNT` | Controle do Timer 1. |
| `0x04000108` | `TM2D` | Valor do Timer 2. |
| `0x0400010A` | `TM2CNT` | Controle do Timer 2. |
| `0x0400010C` | `TM3D` | Valor do Timer 3. |
| `0x0400010E` | `TM3CNT` | Controle do Timer 3. |

### 4.5. Registradores de Interrupção

| Endereço | Nome | Descrição |
| :--- | :--- | :--- |
| `0x04000200` | `IE` | Mascara de interrupções (Interrupt Enable). |
| `0x04000202` | `IF` | Flags de interrupção (Interrupt Flags). |
| `0x04000208` | `IME` | Mestre de interrupção (Interrupt Master Enable). |

## 5. Considerações para o Emulador

### 5.1. Implementação da Memória

- **Acesso à Memória:** O emulador deve modelar corretamente a largura de cada barramento (8, 16 ou 32 bits) e aplicar os wait states apropriados.
- **Função de Leitura/Escrita:** A função de acesso à memória deve verificar a região do endereço e aplicar a lógica específica (ex: leitura da BIOS, acesso à RAM, I/O, etc.).
- **Registradores I/O:** Cada registrador de I/O deve ser modelado como uma estrutura emulada que reage a leituras e escritas. As alterações nesses registradores devem afetar o estado do emulador (display, som, DMA, etc.).

### 5.2. Ciclos e Wait States

A função de acesso à memória deve incluir uma contagem de ciclos. Cada acesso à memória consome um número específico de ciclos, dependendo da região e dos wait states configurados.

```pseudocode
function memory_read(address, size):
    region = get_memory_region(address)
    waitstates = get_waitstates(region)
    cycles = get_cycle_count(size, waitstates)
    cpu.add_cycles(cycles)
    return data
```

### 5.3. O Prefetch Buffer

Embora o prefetch buffer seja um detalhe de hardware relativamente pequeno, ele pode afetar o desempenho. Uma implementação simples pode ignorá-lo, mas uma implementação mais precisa deve modelá-lo.

### 5.4. A BIOS e o Boot

A BIOS está localizada em `0x00000000`. O emulador deve fornecer o conteúdo da BIOS e permitir que a CPU execute a partir dela. O boot deve seguir a sequência correta, que culmina na transferência de controle para o cartucho em `0x08000000`.

## 6. Conclusão

O sistema de memória do GBA é uma peça fundamental para o seu emulador. A compreensão profunda do mapa de memória, da hierarquia de barramentos, dos wait states e dos registradores de I/O permitirá que você emule o hardware com precisão e desempenho. Este documento serve como um guia completo para essa tarefa.