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

  test("shows disconnected UI with mask bar, buttons, detail panel", async ({ terminal }) => {
    await expect(terminal.getByText("Disconnected")).toBeVisible();
    await expect(terminal.getByText("Configure (m)")).toBeVisible();
    await expect(terminal.getByText("Refresh (F5)")).toBeVisible();
    await expect(terminal.getByText("Clear (F6)")).toBeVisible();
    await expect(terminal.getByText("No DTCs found")).toBeVisible();
    await expect(terminal.getByText("Select a DTC from the list")).toBeVisible();
  });

  test("DTC list handles F5 refresh", async ({ terminal }) => {
    terminal.keyPress(Key.F5);
    await expect(terminal.getByText("No DTCs found")).toBeVisible();
  });
});
