import { LitElement, html, css } from 'https://cdn.jsdelivr.net/gh/lit/dist@3.2.1/core/lit-core.min.js';

export class SneakTerminal extends LitElement {

    static properties = {
        history: { type: Array },
        historyIndex: { type: Number },
        entries: { type: Array }
    };

    constructor() {
        console.log('🚨 SneakTerminal constructor');
        super();
        this.history = [];
        this.historyIndex = -1;
        this.entries = [];
        this.addSystemMessage('Terminal initialized. Type "help" for available commands.');
        this.setupEventListeners();
    }

    setupEventListeners() {
        document.addEventListener('command-response', (e) => {
            this.handleCommandResponse(e.detail);
        });
    }


    _handleKeyDown(e) {
        if (e.key === 'Enter' && e.target.value.trim()) {
            this.sendCommand(e.target.value.trim());
            e.target.value = '';
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            this.navigateHistory('up');
        } else if (e.key === 'ArrowDown') {
            e.preventDefault();
            this.navigateHistory('down');
        }
    }

    sendCommand(command) {
        this.history.push(command);
        this.historyIndex = this.history.length;
        
        this.addCommandEntry(command);

        document.dispatchEvent(new CustomEvent('send-command', {
            detail: { command }
        }));

        this.requestUpdate();
    }

    handleCommandResponse(detail) {
        const { response, success } = detail;
        this.addResponseEntry(response, success);
    }

    navigateHistory(direction) {
        const input = this.shadowRoot.querySelector('.terminal-input');
        if (direction === 'up' && this.historyIndex > 0) {
            this.historyIndex--;
            input.value = this.history[this.historyIndex];
        } else if (direction === 'down' && this.historyIndex < this.history.length) {
            this.historyIndex++;
            input.value = this.history[this.historyIndex] || '';
        }
    }

    addCommandEntry(command) {
        this.entries = [...this.entries, {
            type: 'command',
            content: command
        }];
        this.scrollToBottom();
    }

    addResponseEntry(response, success = true) {
        this.entries = [...this.entries, {
            type: 'response',
            content: response,
            error: !success
        }];
        this.scrollToBottom();
    }

    addSystemMessage(message) {
        this.entries = [...this.entries, {
            type: 'system',
            content: message
        }];
        this.scrollToBottom();
    }

    scrollToBottom() {
        this.updateComplete.then(() => {
            const output = this.shadowRoot.querySelector('.terminal-output');
            output.scrollTop = output.scrollHeight;
        });
    }

    clear() {
        this.entries = [];
        this.addSystemMessage('Terminal cleared.');
    }

    render() {
        return html`
            <div class="terminal-container">
                <div class="terminal-output" @scroll=${this._handleScroll}>
                    ${this.entries.map(entry => this.renderEntry(entry))}
                </div>
                <div class="terminal-input-line">
                    <span class="prompt">></span>
                    <input 
                        type="text" 
                        class="terminal-input" 
                        placeholder="Enter command..."
                        @keydown=${this._handleKeyDown}
                    >
                </div>
            </div>
        `;
    }

    renderEntry(entry) {
        entry.content = entry.content.replace(/\r\n\r\n/g, '\n');
        switch (entry.type) {
            case 'command':
                return html`
                    <div class="terminal-entry command">
                        <span class="prompt">> </span>
                        <span>${entry.content}</span>
                    </div>
                `;
            case 'response':
                return html`
                    <div class="terminal-entry response ${entry.error ? 'error' : ''}"
                    >${entry.content}</div>
                `;
            case 'system':
                return html`
                    <div class="terminal-entry system">${entry.content}</div>
                `;
            default:
                return html``;
        }
    }


    static styles = css`
    :host {
        display: block;
        height: 100%;
        max-height: 100vh;
        overflow: hidden;
    }

    .terminal-container {
        background-color: #1e1e1e;
        color: #00ff00;
        font-family: 'Courier New', monospace;
        height: 100%;
        max-height: calc(100vh - 2rem);
        display: flex;
        flex-direction: column;
        border-radius: 5px;
        overflow: hidden;
    }

    .terminal-output {
        flex-grow: 1;
        overflow-y: auto;
        margin-bottom: 1rem;
        padding: 0.5rem;
        max-height: calc(100vh - 6rem);
    }

    .terminal-input-line {
        display: flex;
        align-items: center;
        background-color: #2d2d2d;
        padding: 0.5rem;
        border-radius: 3px;
    }

    .terminal-input {
        background: transparent;
        border: none;
        color: #00ff00;
        font-family: 'Courier New', monospace;
        font-size: 1rem;
        width: 100%;
        margin-left: 0.5rem;
        outline: none;
    }

    .prompt {
        color: #00ff00;
        font-weight: bold;
    }

    .terminal-entry {
        /* margin: 0.25rem 0; */
        word-wrap: break-word;
    }

    .command {
        padding-top: 1rem;
        color: #00ff00;
    }

    .response {
        color: #ffffff;
        /* padding-left: 1rem; */
        white-space: pre;
    }

    .error {
        color: #ff4444;
    }

    .system {
        color: #888888;
        font-style: italic;
    }

    .terminal-output::-webkit-scrollbar {
        width: 8px;
    }

    .terminal-output::-webkit-scrollbar-track {
        background: #2d2d2d;
    }

    .terminal-output::-webkit-scrollbar-thumb {
        background: #888;
        border-radius: 4px;
    }

    .terminal-output::-webkit-scrollbar-thumb:hover {
        background: #555;
    }
`;

}

customElements.define('sneak-terminal', SneakTerminal);