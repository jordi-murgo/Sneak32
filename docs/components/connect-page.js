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
                <ion-header>
                    <ion-toolbar>
                        <ion-title>
                            <ion-icon name="warning-outline" color="warning"></ion-icon>
                            Lost Connection
                        </ion-title>
                        <ion-buttons slot="end">
                            <ion-button @click=${() => this.showDisconnectModal = false}>
                                <ion-icon name="close-outline"></ion-icon>
                            </ion-button>
                        </ion-buttons>
                    </ion-toolbar>
                </ion-header>

                <ion-content class="ion-padding">
                    <div class="disconnect-content">
                        <ion-icon name="bluetooth-outline" color="primary" size="large"></ion-icon>
                        <h2>Connection to your Sneak32 device was lost</h2>
                        
                        <ion-list lines="none">
                            <ion-item>
                                <ion-icon slot="start" name="radio-outline"></ion-icon>
                                <ion-label>Device out of range</ion-label>
                            </ion-item>
                            <ion-item>
                                <ion-icon slot="start" name="battery-dead-outline"></ion-icon>
                                <ion-label>Power loss</ion-label>
                            </ion-item>
                            <ion-item>
                                <ion-icon slot="start" name="wifi-outline"></ion-icon>
                                <ion-label>Bluetooth interference</ion-label>
                            </ion-item>
                        </ion-list>

                        <ion-button 
                            expand="block"
                            @click=${() => this.showDisconnectModal = false}
                            class="ion-margin-top"
                        >
                            <ion-icon slot="start" name="refresh-outline"></ion-icon>
                            Try Reconnecting
                        </ion-button>
                    </div>
                </ion-content>
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

        .disconnect-content {
            display: flex;
            flex-direction: column;
            align-items: center;
            text-align: center;
            padding: 1rem;
        }

        .disconnect-content ion-icon[size="large"] {
            font-size: 48px;
            margin-bottom: 1rem;
        }

        .disconnect-content h2 {
            font-size: 1.2rem;
            margin-bottom: 1.5rem;
            color: var(--ion-color-dark);
        }

        ion-list {
            width: 100%;
            margin-bottom: 1.5rem;
        }

        ion-item {
            --padding-start: 0;
        }

        ion-item ion-icon {
            margin-right: 1rem;
            color: var(--ion-color-medium);
        }
    `;
}

customElements.define('sneak-connect-page', SneakConnectPage); 