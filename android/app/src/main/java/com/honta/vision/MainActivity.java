package com.honta.vision;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity {
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private TextView status;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(32, 32, 32, 32);

        Button runSmoke = new Button(this);
        runSmoke.setText("Run JNI smoke test");
        runSmoke.setOnClickListener(view -> runSmokeTest());
        root.addView(runSmoke, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        status = new TextView(this);
        status.setPadding(32, 32, 32, 32);
        status.setTextSize(16);
        status.setText(HontaNative.bridgeVersion());

        ScrollView scrollView = new ScrollView(this);
        scrollView.addView(status);
        root.addView(scrollView, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                0,
                1.0f));

        setContentView(root);
        mainHandler.postDelayed(this::runSmokeTest, 500L);
    }

    @Override
    protected void onDestroy() {
        executor.shutdownNow();
        HontaNative.release();
        super.onDestroy();
    }

    private void runSmokeTest() {
        status.setText("Running JNI smoke test...\n");
        executor.submit(() -> {
            StringBuilder log = new StringBuilder();
            try {
                File config = prepareSmokeConfig(log);
                step(log, "init", () -> HontaNative.init(config.getAbsolutePath()));
                step(log, "setTopCoverModel(A001)", () -> HontaNative.setTopCoverModel("A001"));
                step(log, "getOverlayRtspUrl", () -> {
                    String url = HontaNative.getOverlayRtspUrl();
                    append(log, "  overlay=" + url);
                });
                step(log, "startDetect(entrance_hole)",
                        () -> HontaNative.startDetect("entrance_hole"));
                retryStep(log, "confirmEntranceCandidate(candidate 1)",
                        () -> HontaNative.confirmEntranceCandidate(
                                1, 1, 1800.0, 4.115, 1.0, 0.0));
                step(log, "startDetect(center_horn)",
                        () -> HontaNative.startDetect("center_horn"));
                retryStep(log, "confirmCenterCandidate(candidate 1)",
                        () -> HontaNative.confirmCenterCandidate(
                                1, 1800.0, 2.0, 1.0, 0.0));
                step(log, "calculateAllHorns", HontaNative::calculateAllHorns);
                step(log, "getHornCoordinate(1)", () -> {
                    HontaNative.HornCoordinate coordinate = HontaNative.getHornCoordinate(1);
                    append(log, "  horn1=(" + coordinate.x + ", " + coordinate.y + ")");
                });
                step(log, "release", HontaNative::release);
                append(log, "JNI smoke test: PASS");
            } catch (Throwable error) {
                append(log, "JNI smoke test: FAIL");
                append(log, error.getClass().getSimpleName() + ": " + error.getMessage());
            }
            mainHandler.post(() -> status.setText(log.toString()));
        });
    }

    private File prepareSmokeConfig(StringBuilder log) throws IOException {
        File root = new File(getFilesDir(), "phase7_smoke");
        copyAsset("phase7_smoke/runtime.json", new File(root, "runtime.json"));
        copyAsset("phase7_smoke/camera/top_rgb_camera.json",
                new File(root, "camera/top_rgb_camera.json"));
        copyAsset("phase7_smoke/top_cover/A001.json", new File(root, "top_cover/A001.json"));
        append(log, "config=" + new File(root, "runtime.json").getAbsolutePath());
        return new File(root, "runtime.json");
    }

    private void copyAsset(String assetPath, File target) throws IOException {
        File parent = target.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("failed to create " + parent);
        }
        try (InputStream input = getAssets().open(assetPath);
             FileOutputStream output = new FileOutputStream(target)) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = input.read(buffer)) != -1) {
                output.write(buffer, 0, read);
            }
        }
    }

    private void step(StringBuilder log, String name, SmokeStep step) {
        append(log, name + "...");
        step.run();
        append(log, "  OK");
    }

    private void retryStep(StringBuilder log, String name, SmokeStep step) {
        append(log, name + "...");
        RuntimeException lastError = null;
        for (int attempt = 1; attempt <= 20; ++attempt) {
            try {
                step.run();
                append(log, "  OK attempt=" + attempt);
                return;
            } catch (RuntimeException error) {
                lastError = error;
                append(log, "  waiting attempt=" + attempt + " error=" + error.getMessage());
                try {
                    Thread.sleep(500L);
                } catch (InterruptedException interrupted) {
                    Thread.currentThread().interrupt();
                    throw new RuntimeException("interrupted while waiting for detection candidate", interrupted);
                }
            }
        }
        throw lastError != null ? lastError : new RuntimeException("candidate was not confirmed");
    }

    private void append(StringBuilder log, String line) {
        log.append(line).append('\n');
    }

    private interface SmokeStep {
        void run();
    }
}
