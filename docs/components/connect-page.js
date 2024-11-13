import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';
import './sneak-esp32-logo.js';

export class SneakConnectPage extends LitElement {
    static properties = {
        isConnected: { 
            type: Boolean,
            attribute: 'is-connected'
        },
        showDisconnectModal: { 
            type: Boolean,
            attribute: 'show-disconnect-modal'
        }
    };

    constructor() {
        super();
        console.log('🏗️ Initializing SneakConnectPage');
        this.isConnected = false;
        this.showDisconnectModal = false;
        this.connectionInProgress = false;
        
        // Listen for successful connections
        document.addEventListener('device-connected', (event) => {
            console.log('📱 Device connected event received:', event.detail);
            this.isConnected = event.detail.isConnected;
            
            if (this.isConnected) {
                this.showToast(
                    `Connected to ${event.detail.deviceName}.`,
                    'warning'
                );
            }
        });

        // Listen for disconnection events
        document.addEventListener('disconnected', (event) => {
            console.log('📢 Disconnect event received:', event.detail);
            this.isConnected = false;
            if (event.detail.unexpected) {
                this.showDisconnectModal = true;
            }
        });
    }

    render() {
        return html`
            <ion-page class="ion-page">
                <ion-header>
                    <ion-toolbar>
                        <ion-title>🥷 Sneak32 Manager</ion-title>
                    </ion-toolbar>
                </ion-header>

                <ion-content class="ion-content">
                    <div class="flex-col-center">
                        <sneak-esp32-logo></sneak-esp32-logo>
                        <ion-button @click=${this.handleSearch}>
                            Search for Sneak32
                        </ion-button>
                    </div>
                </ion-content>

                ${this.renderDisconnectModal()}
            </ion-page>
        `;
    }

    renderDisconnectModal() {
        return html`
            <ion-modal 
                .isOpen=${this.showDisconnectModal}
                @didDismiss=${() => this.showDisconnectModal = false}
            >
                <ion-card>
                    <ion-card-header>
                        <ion-card-title>Unexpected Disconnection</ion-card-title>
                    </ion-card-header>
                    <ion-card-content>
                        <p>The connection to your Sneak32 device was unexpectedly lost. This could be due to:</p>
                        <ul>
                            <li>Device moving out of range</li>
                            <li>Device power loss</li>
                            <li>Interference in the Bluetooth connection</li>
                        </ul>
                        <p>Please try reconnecting to your device.</p>
                        <ion-button 
                            expand="block"
                            @click=${() => this.showDisconnectModal = false}
                        >
                            Close
                        </ion-button>
                    </ion-card-content>
                </ion-card>
            </ion-modal>
        `;
    }

    async handleSearch() {
        if (this.connectionInProgress) {
            console.log('🚫 Connection already in progress, ignoring request');
            return;
        }

        console.log('🔍 Requesting connection...');
        this.connectionInProgress = true;

        try {
            // Dispatch connection request event
            this.dispatchEvent(new CustomEvent('connection-requested', {
                bubbles: true,
                composed: true
            }));
        } catch (error) {
            console.error('❌ Error requesting connection:', error);
            this.showToast('Failed to request connection', 'danger');
        } finally {
            this.connectionInProgress = false;
        }
    }

    showToast(message, color = 'primary') {
        console.log('🔔 Showing toast:', message, color);
        const toast = document.createElement('ion-toast');
        toast.message = message;
        toast.duration = 3000;
        toast.position = 'bottom';
        toast.color = color;
        
        document.body.appendChild(toast);
        toast.present();
    }

    static styles = css`
            :host {
            display: block;
            height: 100%;
        }

        .flex-col-center {
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: flex-start;
            padding: 1rem;
            padding-top: 40px;
        }

        .logo-container {
            margin-bottom: 2rem;
            max-width: 200px;
        }

        .logo-container img {
            width: 130px;
            height: auto;
            border-radius: 18px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.4);
        }

        ion-card {
            margin: 1rem;
        }

        ion-card-title {
            text-align: center;
        }

        ul {
            padding-left: 1.5rem;
        }

        ion-modal::part(content) {
            --height: auto;
        }
    `;
}

customElements.define('sneak-connect-page', SneakConnectPage); 