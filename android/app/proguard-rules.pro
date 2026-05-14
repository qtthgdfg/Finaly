# Monero Miner ProGuard Rules

# Keep JNI methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep mining service
-keep class com.monerominer.MinerService { *; }
-keep class com.monerominer.MinerConfig { *; }

# Keep RandomX native calls
-keep class randomx.** { *; }

# Strip debug info in release
-keepattributes SourceFile,LineNumberTable
-renamesourcefileattribute SourceFile

# Optimization
-optimizationpasses 5
-dontusemixedcaseclassnames
-dontskipnonpubliclibraryclasses
-dontpreverify
-verbose

# Remove logging in release
-assumenosideeffects class android.util.Log {
    public static boolean isLoggable(java.lang.String, int);
    public static int v(...);
    public static int d(...);
}
