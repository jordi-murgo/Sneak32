import { LitElement, html, css } from 'https://cdn.jsdelivr.net/npm/lit-element@4.1.1/+esm';
import { alertController } from 'https://cdn.jsdelivr.net/npm/@ionic/core/dist/ionic/index.esm.js';

export class SneakSideMenu extends LitElement {
    static properties = {
        currentPage: {
            type: String,
            attribute: 'current-page'
        },
        menuItems: {
            type: Array,
            attribute: 'menu-items'
        },
        actionItems: {
            type: Array,
            attribute: 'action-items'
        },
        contentId: {
            type: String,
            attribute: 'content-id'
        },
    };

    constructor() {
        super();
        this.currentPage = 'Device Info';
        this.menuItems = [
            { title: 'Device Info', icon: 'hardware-chip' },
            { title: 'Data Dashboard', icon: 'bar-chart' },
            { title: 'WiFi Networks', icon: 'wifi' },
            { title: 'WiFi Devices', icon: 'phone-portrait' },
            { title: 'BLE Devices', icon: 'bluetooth' },
            { title: 'Setup', icon: 'settings' },
            { title: 'Terminal', icon: 'terminal' },
        ];
        this.actionItems = [
            {
                title: 'Save Captured Data',
                icon: 'save',
                color: 'success',
                action: 'save_data'
            },
            {
                title: 'Clear Captured Data',
                icon: 'trash',
                color: 'warning',
                action: 'clear_data'
            },
            {
                title: 'Restart Device',
                icon: 'refresh',
                color: 'warning',
                action: 'reset_device'
            }
        ];
    }

    render() {
        return html`
            <ion-header>
                <ion-toolbar>
                    <ion-title>Menu</ion-title>
                </ion-toolbar>
            </ion-header>
            
            <ion-content>
                <ion-list lines="none">
                    ${this.renderNavigationItems()}
                    <ion-item></ion-item>
                    ${this.renderActionItems()}
                    <ion-item></ion-item>
                    ${this.renderDisconnectButton()}
                </ion-list>
            </ion-content>
            
            ${this.renderFooter()}
        `;
    }

    renderNavigationItems() {
        return html`
            ${this.menuItems.map(item => this.renderMenuItem(item))}
        `;
    }

    renderMenuItem(item) {
        const isSelected = this.currentPage === item.title;
        return html`
            <ion-item 
                button 
                ?selected=${isSelected}
                @click=${() => this.handlePageChange(item.title)}
            >
                <ion-icon slot="start" name="${item.icon}"></ion-icon>
                <ion-label>${item.title}</ion-label>
            </ion-item>
        `;
    }

    renderActionItems() {
        return html`
            ${this.actionItems.map(item => this.renderActionItem(item))}
        `;
    }

    renderActionItem(item) {
        return html`
            <ion-item 
                button 
                @click=${() => this.handleAction(item.action)}
                color="${item.color}"
            >
                <ion-icon slot="start" name="${item.icon}"></ion-icon>
                <ion-label>${item.title}</ion-label>
            </ion-item>
        `;
    }

    renderDisconnectButton() {
        return html`
            <ion-item 
                button 
                @click=${this.handleDisconnect}
                color="primary"
            >
                <ion-icon slot="start" name="power"></ion-icon>
                <ion-label>Disconnect</ion-label>
            </ion-item>
        `;
    }

    renderFooter() {
        return html`
            <ion-footer>
                <ion-toolbar>
                    <ion-title size="small">&copy; 2024 Jordi Murgo.</ion-title>
                </ion-toolbar>
            </ion-footer>
        `;
    }

    async handlePageChange(page) {
        console.log('handlePageChange', page);
        this.currentPage = page;
        this.dispatchEvent(new CustomEvent('page-change', {
            detail: { page },
            bubbles: true,
            composed: true
        }));
        // Add this new code to close menu automatically
        const menuController = document.querySelector('ion-menu');
        if (window.innerWidth < 768 && menuController) {  // Check if we're in mobile view
            await menuController.close();
        }
    }

    handleAction(action) {
        switch (action) {
            case 'save_data':
                this.dispatchEvent(new CustomEvent('save-device-data', {
                    bubbles: true,
                    composed: true
                }));
                break;
            case 'clear_data':
                this.handleClearData();
                break;
            case 'reset_device':
                this.handleResetDevice();
                break;
        }
    }

    async handleClearData() {
        const alert = await alertController.create({
            header: 'Clear Data',
            message: 'Are you sure you want to clear all captured data?',
            buttons: [
                {
                    text: 'Cancel',
                    role: 'cancel'
                },
                {
                    text: 'Clear',
                    role: 'destructive',
                    handler: () => {
                        this.dispatchEvent(new CustomEvent('clear-device-data', {
                            bubbles: true,
                            composed: true,
                        }));
                    }
                }
            ]
        });

        await alert.present();
    }

    async handleResetDevice() {
        const alert = await alertController.create({
            header: 'Reset Device',
            message: 'Are you sure you want to restart the device?',
            buttons: [
                {
                    text: 'Cancel',
                    role: 'cancel'
                },
                {
                    text: 'Reset',
                    role: 'destructive',
                    handler: async () => {
                        await alert.dismiss();
                        this.dispatchEvent(new CustomEvent('reset-device', {
                            bubbles: true,
                            composed: true
                        }));
                    }
                }
            ]
        });

        await alert.present();
    }

    async handleDisconnect() {
        const alert = await alertController.create({
            header: 'Disconnect',
            message: 'Are you sure you want to disconnect from the device?',
            buttons: [
                {
                    text: 'Cancel',
                    role: 'cancel'
                },
                {
                    text: 'Disconnect',
                    role: 'destructive',
                    handler: () => {
                        this.dispatchEvent(new CustomEvent('disconnect-device', {
                            bubbles: true,
                            composed: true,
                        }));
                    }
                }
            ]
        });

        await alert.present();
    }

    static styles = css`
        :host {
            display: flex;
            flex-direction: column;
            height: 100%;
        }

        ion-header {
            flex-shrink: 0;
        }

        ion-content {
            flex: 1;
            overflow: auto;
        }

        ion-footer {
            flex-shrink: 0;
        }

        /* Asegura que el contenido del ion-content se ajuste correctamente */
        ion-content::part(scroll) {
            display: flex;
            flex-direction: column;
        }

        ion-list {
            flex: 1;
            overflow: auto;
            margin: 0;
            padding: 0;
        }

        /* Resto de los estilos existentes */
        ion-item {
            --padding-start: 16px;
            --ion-item-background: transparent;
            --ion-item-color: var(--ion-color-medium);
            --ion-item-hover-background: var(--ion-color-light);
        }

        ion-item[selected] {
            --ion-item-color: var(--ion-color-primary);
            --ion-item-background: var(--ion-color-primary-transparent);
        }

        ion-item ion-icon {
            color: var(--ion-item-color);
        }

        ion-footer ion-title {
            font-size: 0.8rem;
            color: var(--ion-color-medium);
        }
    `;
}

customElements.define('sneak-side-menu', SneakSideMenu); 