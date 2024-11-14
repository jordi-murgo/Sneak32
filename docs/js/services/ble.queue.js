class TimeoutError extends Error {
  constructor(message) {
    super(message);
    this.name = 'TimeoutError';
  }
}

class BLEOperationQueue {
  constructor() {
    this.queue = [];
    this.isProcessing = false;
    this.OPERATION_TIMEOUT = 1000;
  }

  async enqueue(operation, description) {
    return new Promise((resolve, reject) => {
      const operationItem = {
        execute: operation,
        resolve,
        reject,
        description
      };

      this.queue.push(operationItem);

      console.log(`🚧 [BLEQueue] Enqueuing operation: ${description || 'Unnamed'}. Queue size: ${this.queue.length}`);

      this.processQueue();
    });
  }

  processQueue() {
    if (this.isProcessing) return;
    if (this.queue.length === 0) return;

    this.isProcessing = true;
    const operationItem = this.queue.shift();

    const timeout = setTimeout(() => {
      console.log(`❌ [BLEQueue] Operation timed out: ${operationItem.description}`);
      this.isProcessing = false;
      operationItem.reject(new TimeoutError('Operation timed out'));
      this.processQueue();
    }, this.OPERATION_TIMEOUT);

    console.log(`🔄 [BLEQueue] Executing operation: ${operationItem.description}`);
    operationItem.execute()
      .then(result => {
        console.log(`✅ [BLEQueue] Operation completed: ${operationItem.description}. Queue size: ${this.queue.length}`);
        operationItem.resolve(result);
      })
      .catch(error => {
        console.log(`❌ [BLEQueue] Operation failed: ${operationItem.description}. Queue size: ${this.queue.length}`);
        operationItem.reject(error);
      }).finally(() => {
        this.isProcessing = false;
        clearTimeout(timeout);
        this.processQueue();
      });
  }

  clear() {
    console.log(`🔄 [BLEQueue] Clearing queue. Current size: ${this.queue.length}`);
    this.queue = [];
    this.isProcessing = false;
  }
}

export const bleOperationQueue = new BLEOperationQueue(); 