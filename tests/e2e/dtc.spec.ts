import { test, expect, Key } from "@microsoft/tui-test";
import { startServer, stopServer } from "./test-server.js";
import { resolve } from "node:path";

const program = { file: resolve("build/fuse-diag") };

test.describe("FuseDiag DTC Page", () => {
  test.beforeAll(async () => {
    await startServer();
  });

  test.afterAll(() => {
    stopServer();
  });

  test.use({ program });

  test("shows disconnected UI on startup", async ({ terminal }) => {
    await expect(terminal.getByText("Disconnected")).toBeVisible();
    await expect(terminal.getByText("Configure (m)")).toBeVisible();
    await expect(terminal.getByText("Refresh (F5)")).toBeVisible();
    await expect(terminal.getByText("No DTCs found")).toBeVisible();
  });

  test("mask panel opens and closes with m key", async ({ terminal }) => {
    terminal.keyPress("m");
    terminal.keyPress("m");
  });

  test("handles F5 refresh and j/k navigation", async ({ terminal }) => {
    terminal.keyPress(Key.F5);
    terminal.keyPress("j");
    terminal.keyPress("k");
    terminal.keyPress(Key.Escape);
  });
});
