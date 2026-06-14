import { test, expect, Key } from "@microsoft/tui-test";
import { startServer, stopServer } from "./test-server.js";
import { resolve } from "node:path";

const program = { file: resolve("build/fuse-diag") };

test.describe("FuseDiag Snapshots Tab", () => {
  test.beforeAll(async () => {
    await startServer();
  });

  test.afterAll(() => {
    stopServer();
  });

  test.use({ program });

  test("DTC page has both DTC List and Snapshots tabs", async ({ terminal }) => {
    await expect(terminal.getByText("Snapshots")).toBeVisible();
    await expect(terminal.getByText("Configure (m)")).toBeVisible();
    await expect(terminal.getByText("No DTCs found")).toBeVisible();
  });

  test("F5 refreshes on Snapshots tab", async ({ terminal }) => {
    terminal.keyPress(Key.F5);
    await expect(terminal.getByText("Snapshots")).toBeVisible();
  });
});
