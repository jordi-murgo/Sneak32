import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';

export class SneakDataTransfer extends LitElement {
    static properties = {
        data: { type: Array },
        isLoading: { type: Boolean }
    };

    constructor() {
        super();
        this.data = [];
        this.isLoading = false;
    }

    render() {
        return html`
            <div class="data-transfer-content">
                <ion-list>
                    <ion-item>
                        <ion-label>
                            <h2>Captured Data</h2>
                            <p>Total records: ${this.data.length}</p>
                        </ion-label>
                    </ion-item>

                    ${this.isLoading ? 
                        html`
                            <ion-item>
                                <ion-spinner></ion-spinner>
                                <ion-label>Loading data...</ion-label>
                            </ion-item>
                        ` : 
                        this.renderDataList()
                    }
                </ion-list>

                <div class="button-container">
                    <ion-button expand="block" @click=${this.downloadData}>
                        <ion-icon slot="start" name="download-outline"></ion-icon>
                        Download Data
                    </ion-button>
                </div>
            </div>
        `;
    }

    renderDataList() {
        return this.data.map(item => html`
            <ion-item>
                <ion-label>
                    <h3>${item.type}</h3>
                    <p>${item.timestamp}</p>
                    <p>${item.details}</p>
                </ion-label>
            </ion-item>
        `);
    }

    downloadData() {
        const dataStr = JSON.stringify(this.data, null, 2);
        const blob = new Blob([dataStr], { type: 'application/json' });
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `sneak32-data-${new Date().toISOString()}.json`;
        a.click();
        window.URL.revokeObjectURL(url);
    }

    static styles = css`
        :host {
            display: block;
            padding: 1rem;
        }

        .data-transfer-content {
            max-width: 800px;
            margin: 0 auto;
        }

        .button-container {
            margin-top: 2rem;
        }

        ion-spinner {
            margin-right: 1rem;
        }
    `;
}

customElements.define('sneak-data-transfer', SneakDataTransfer); 