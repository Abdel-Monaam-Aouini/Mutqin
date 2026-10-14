# R8 rules for the release build.
# The native library (jni_bridge.cpp) looks these up by name, so shrinking and renaming must leave them alone.
-keep class app.mutqin.core.MutqinNative {
    native <methods>;
}
-keep class app.mutqin.core.MutqinException {
    <init>(int, java.lang.String);
}
