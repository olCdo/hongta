package com.honta.vision;

public final class AmrPoseSnapshot {
    public final double x;
    public final double y;
    public final double yaw;
    public final long timestampMillis;
    public final String fsm;
    public final boolean localizationError;
    public final boolean collision;

    public AmrPoseSnapshot(
            double x,
            double y,
            double yaw,
            long timestampMillis,
            String fsm,
            boolean localizationError,
            boolean collision) {
        this.x = x;
        this.y = y;
        this.yaw = yaw;
        this.timestampMillis = timestampMillis;
        this.fsm = fsm;
        this.localizationError = localizationError;
        this.collision = collision;
    }
}
