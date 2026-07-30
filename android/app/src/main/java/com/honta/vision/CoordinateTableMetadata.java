package com.honta.vision;

import java.util.Objects;

public final class CoordinateTableMetadata {
    public final String mapId;
    public final String topCoverModel;
    public final String coordinateAlgorithmVersion;
    public final String runtimeConfigHash;
    public final String cameraConfigHash;
    public final String topCoverDataHash;
    public final long calibratedAtMillis;

    public CoordinateTableMetadata(
            String mapId,
            String topCoverModel,
            String coordinateAlgorithmVersion,
            String runtimeConfigHash,
            String cameraConfigHash,
            String topCoverDataHash,
            long calibratedAtMillis) {
        this.mapId = mapId;
        this.topCoverModel = topCoverModel;
        this.coordinateAlgorithmVersion = coordinateAlgorithmVersion;
        this.runtimeConfigHash = runtimeConfigHash;
        this.cameraConfigHash = cameraConfigHash;
        this.topCoverDataHash = topCoverDataHash;
        this.calibratedAtMillis = calibratedAtMillis;
    }

    public boolean isCompatibleWith(CoordinateTableMetadata current) {
        if (current == null) {
            return false;
        }
        return Objects.equals(mapId, current.mapId)
                && Objects.equals(topCoverModel, current.topCoverModel)
                && Objects.equals(coordinateAlgorithmVersion, current.coordinateAlgorithmVersion)
                && Objects.equals(runtimeConfigHash, current.runtimeConfigHash)
                && Objects.equals(cameraConfigHash, current.cameraConfigHash)
                && Objects.equals(topCoverDataHash, current.topCoverDataHash);
    }
}
