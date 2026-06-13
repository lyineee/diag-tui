import { test, expect, Key } from "@microsoft/tui-test";
import { startServer, stopServer } from "./test-server.js";
import { resolve } from "node:path";

const program = { file: resolve("build/fuse-diag") };

test.describe("FuseDiag Navigation", () => {
  test.beforeAll(async () => {
    await startServer();
  });

  test.afterAll(() => {
    stopServer();
  });

  test.use({ program });

  test("nav menu shows all sections", async ({ terminal }) => {
    await expect(terminal.getByText("Raw Send")).toBeVisible();
    await expect(terminal.getByText("Session")).toBeVisible();
    await expect(terminal.getByText("Settings")).toBeVisible();
  });

  test("Tab switches from DTC to DID page", async ({ terminal }) => {
    await expect(terminal.getByText("No DTCs found")).toBeVisible();

    terminal.keyPress(Key.Tab);

    await expect(terminal.getByText("Database")).toBeVisible();
    await expect(terminal.getByText("Browser")).toBeVisible();
  });

  test("F2 directly opens DID page", async ({ terminal }) => {
    terminal.keyPress(Key.F2);

    await expect(terminal.getByText("Database")).toBeVisible();
    await expect(terminal.getByText("Browser")).toBeVisible();
    await expect(terminal.getByText("Polling")).toBeVisible();
  });

  test("F3 opens Raw Send page", async ({ terminal }) => {
    terminal.keyPress(Key.F3);

    await expect(terminal.getByText("Raw Send")).toBeVisible();
  });
});
