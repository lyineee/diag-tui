import { ChildProcess, spawn } from "child_process";

let serverProcess: ChildProcess | null = null;

export async function startServer(): Promise<void> {
  return new Promise((resolve, reject) => {
    serverProcess = spawn("./build/test-doip-server", [], {
      stdio: ["ignore", "pipe", "pipe"],
    });

    const timeout = setTimeout(() => {
      resolve();
    }, 1500);

    serverProcess.stdout?.on("data", (data: Buffer) => {
      if (data.toString().includes("TCP listening")) {
        clearTimeout(timeout);
        resolve();
      }
    });

    serverProcess.on("error", reject);
    serverProcess.on("exit", (code) => {
      if (code !== null && code !== 0) {
        reject(new Error(`Server exited with code ${code}`));
      }
    });
  });
}

export function stopServer(): void {
  if (serverProcess) {
    serverProcess.kill("SIGTERM");
    serverProcess = null;
  }
}
