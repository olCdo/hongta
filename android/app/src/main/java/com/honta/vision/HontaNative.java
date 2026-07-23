package com.honta.vision;

public final class HontaNative {
    public static final int OK = 0;

    static {
        System.loadLibrary("honta_native");
        System.loadLibrary("honta_jni");
    }

    private HontaNative() {}

    public static native String bridgeVersion();

    public static void init(String configPath) {
        requireOk(nativeInit(configPath), "honta_init");
    }

    public static void setTopCoverModel(String modelId) {
        requireOk(nativeSetTopCoverModel(modelId), "honta_set_top_cover_model");
    }

    public static void startDetect(String detectType) {
        requireOk(nativeStartDetect(detectType), "honta_start_detect");
    }

    public static String getOverlayRtspUrl() {
        return nativeGetOverlayRtspUrl();
    }

    public static void stopDetect() {
        requireOk(nativeStopDetect(), "honta_stop_detect");
    }

    public static void confirmEntranceCandidate(
            int candidateId,
            int entranceHoleNo,
            double heightMm,
            double amrX,
            double amrY,
            double amrYaw) {
        requireOk(
                nativeConfirmEntranceCandidate(
                        candidateId, entranceHoleNo, heightMm, amrX, amrY, amrYaw),
                "honta_confirm_entrance_candidate");
    }

    public static void confirmCenterCandidate(
            int candidateId,
            double heightMm,
            double amrX,
            double amrY,
            double amrYaw) {
        requireOk(
                nativeConfirmCenterCandidate(candidateId, heightMm, amrX, amrY, amrYaw),
                "honta_confirm_center_candidate");
    }

    public static void calculateAllHorns() {
        requireOk(nativeCalculateAllHorns(), "honta_calculate_all_horns");
    }

    public static HornCoordinate getHornCoordinate(int hornNo) {
        double[] values = nativeGetHornCoordinate(hornNo);
        return new HornCoordinate(hornNo, values[0], values[1]);
    }

    public static String getLastError() {
        return nativeGetLastError();
    }

    public static void release() {
        nativeRelease();
    }

    private static void requireOk(int status, String callName) {
        if (status != OK) {
            String error = nativeGetLastError();
            throw new HontaNativeException(status, callName + " failed: " + error);
        }
    }

    private static native int nativeInit(String configPath);

    private static native int nativeSetTopCoverModel(String modelId);

    private static native int nativeStartDetect(String detectType);

    private static native String nativeGetOverlayRtspUrl();

    private static native int nativeStopDetect();

    private static native int nativeConfirmEntranceCandidate(
            int candidateId,
            int entranceHoleNo,
            double heightMm,
            double amrX,
            double amrY,
            double amrYaw);

    private static native int nativeConfirmCenterCandidate(
            int candidateId,
            double heightMm,
            double amrX,
            double amrY,
            double amrYaw);

    private static native int nativeCalculateAllHorns();

    private static native double[] nativeGetHornCoordinate(int hornNo);

    private static native String nativeGetLastError();

    private static native void nativeRelease();

    public static final class HornCoordinate {
        public final int hornNo;
        public final double x;
        public final double y;

        public HornCoordinate(int hornNo, double x, double y) {
            this.hornNo = hornNo;
            this.x = x;
            this.y = y;
        }
    }

    public static final class HontaNativeException extends RuntimeException {
        public final int status;

        public HontaNativeException(String message) {
            super(message);
            this.status = -1;
        }

        public HontaNativeException(int status, String message) {
            super(message);
            this.status = status;
        }
    }
}
