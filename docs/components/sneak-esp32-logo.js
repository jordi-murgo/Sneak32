import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';

export class SneakEsp32Logo extends LitElement {
    static styles = css`
        :host {
            display: block;
            --ninja-speed: 4s;
            --ninja-size: 24px;
        }

        .sneak32-logo {
            margin: 0px auto 30px;
            position: relative;
        }

        .sneak32-logo img {
            width: 100%;
            height: auto;
            box-shadow: 0px 4px 8px rgba(0, 0, 0, 0.4);
            border-radius: 18px;
            object-fit: contain;
        }

        .sneak32-logo::after {
            content: '🥷';
            position: absolute;
            font-size: var(--ninja-size);
            top: 8%;
            left: -20px;
            animation: ninjaSlide var(--ninja-speed) infinite;
            opacity: 0;
        }

        @keyframes ninjaSlide {
            0% {
                opacity: 0;
                left: calc(-1 * var(--ninja-size) / 4);
            }

            40% {
                opacity: 1;
                left: calc(50% - var(--ninja-size) / 2);
            }
            60% {
                opacity: 1;
                left: calc(50% - var(--ninja-size) / 2);
            }

            100% {
                opacity: 0;
                left: calc(-1 * var(--ninja-size) / 4);
            }
        }
    `;

    render() {
        return html`
            <div class="sneak32-logo">
                <img src="logo-esp32.png" alt="ESP32 Logo" />
            </div>
        `;
    }
}

customElements.define('sneak-esp32-logo', SneakEsp32Logo); 