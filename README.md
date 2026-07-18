# ESP32 Matrix Display

Firmware para ESP32 que controla uma matriz de LED (MAX7219/MAX72XX, via
[MD_MAX72XX](https://github.com/MajicDesigns/MD_MAX72XX)) e a transforma em um
display conectado ao WiFi, controlável por uma interface web e por uma API
REST simples. Pensado para crescer além de mensagens de texto — próximos
passos incluem um modo relógio e outros widgets.

## Recursos

- **Interface web** para digitar e enviar mensagens de rolagem (scroll) para o display.
- **API REST** (`GET /api/message?msg=...`) para enviar mensagens programaticamente.
- **Autenticação opcional por API key** (header `X-API-Key`), gerada e gerenciável pela interface web.
- **Portal de configuração de WiFi**: se a rede configurada não for encontrada, o
  dispositivo sobe um Access Point (`MD-Display-Setup`) para você configurar
  SSID/senha pelo navegador.
- **Configurações persistentes** (velocidade de scroll, brilho, modo de exibição)
  salvas na NVS via `Preferences`, sobrevivendo a reinícios.
- **Ícones customizados** embutidos, inseridos na mensagem com tags como
  `[heart]`, `[wifi]`, `[smile]`, `[clock]`, `[star]`, entre outros.
- **Modo estático com blink**, além do modo scroll padrão.
- **Reset de fábrica**: segurar o botão BOOT ao ligar/reiniciar apaga
  configurações e credenciais salvas.

## Hardware

Testado com ESP32 + módulo de matriz de LED FC16 (MAX7219), 4 módulos em cadeia.

| Matriz (MAX7219) | ESP32 (SPI de hardware / VSPI) |
|---|---|
| Vcc      | 3.3V |
| GND      | GND |
| DIN      | GPIO 23 (VSPI MOSI) |
| CS / LD  | GPIO 5  (VSPI SS) |
| CLK      | GPIO 18 (VSPI SCK) |

Os pinos podem ser ajustados no início do `.ino` (`CLK_PIN`, `DATA_PIN`, `CS_PIN`).

## Dependências

- [Arduino core para ESP32](https://github.com/espressif/arduino-esp32)
- [MD_MAX72XX](https://github.com/MajicDesigns/MD_MAX72XX) (biblioteca de controle da matriz)

## Primeiro uso

1. Compile e grave o firmware no ESP32 (Arduino IDE ou `arduino-cli`).
2. Na primeira inicialização (sem WiFi configurado), o dispositivo sobe o
   Access Point **MD-Display-Setup**. Conecte-se a ele e acesse o IP exibido
   no display (ou `192.168.4.1`) pelo navegador.
3. Configure o SSID/senha da sua rede pela interface web. O dispositivo reinicia
   e tenta se conectar; se falhar, volta ao modo de configuração.
4. Após conectado, o próprio display mostra o IP atribuído — acesse-o pelo
   navegador para enviar mensagens, ajustar brilho/velocidade e gerenciar a API key.

## API REST

```
GET /api/message?msg=Hello%20World
```

Se a autenticação estiver habilitada, envie a API key no header:

```
X-API-Key: <chave gerada na interface web>
```

## Reset de fábrica

Segure o botão **BOOT** do ESP32 durante a energização/reset para apagar
todas as configurações salvas (WiFi, brilho, velocidade, API key) e voltar
ao estado de fábrica.

## Roadmap

- [ ] Modo relógio (NTP)
- [ ] Outros widgets/modos de exibição

## Licença

Defina a licença do projeto aqui (ex.: MIT).
