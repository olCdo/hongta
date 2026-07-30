package com.honta.vision;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

public final class HontaNativeController implements AutoCloseable {
    private final ExecutorService nativeExecutor = Executors.newSingleThreadExecutor();
    private State state = State.RELEASED;

    public Future<?> init(String configPath) {
        return submit(() -> {
            if (state != State.RELEASED) {
                HontaNative.release();
                state = State.RELEASED;
            }
            HontaNative.init(configPath);
            state = State.INITIALIZED;
        });
    }

    public Future<?> setTopCoverModel(String modelId) {
        return submit(() -> {
            requireAtLeast(State.INITIALIZED, "setTopCoverModel");
            HontaNative.setTopCoverModel(modelId);
        });
    }

    public Future<?> startDetect(String detectType) {
        return submit(() -> {
            requireAtLeast(State.INITIALIZED, "startDetect");
            if (state == State.DETECTING) {
                HontaNative.stopDetect();
                state = State.INITIALIZED;
            }
            HontaNative.startDetect(detectType);
            state = State.DETECTING;
        });
    }

    public Future<?> stopDetect() {
        return submit(() -> {
            if (state == State.DETECTING) {
                HontaNative.stopDetect();
                state = State.INITIALIZED;
            }
        });
    }

    public Future<?> release() {
        return submit(() -> {
            HontaNative.release();
            state = State.RELEASED;
        });
    }

    public synchronized State state() {
        return state;
    }

    @Override
    public void close() {
        try {
            release().get();
        } catch (Exception ignored) {
            HontaNative.release();
        } finally {
            nativeExecutor.shutdownNow();
        }
    }

    private Future<?> submit(NativeAction action) {
        return nativeExecutor.submit(() -> {
            synchronized (this) {
                action.run();
            }
        });
    }

    private void requireAtLeast(State required, String operation) {
        if (state.ordinal() < required.ordinal()) {
            throw new IllegalStateException(operation + " requires " + required + ", current=" + state);
        }
    }

    private interface NativeAction {
        void run();
    }

    public enum State {
        RELEASED,
        INITIALIZED,
        DETECTING
    }
}
