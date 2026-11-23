plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "de.willi.openintercom"
    compileSdk {
        version = release(37)
    }

    defaultConfig {
        applicationId = "de.willi.openintercom.phone"
        minSdk = 26
        targetSdk = 36
        versionCode = 5
        versionName = "1.4-phone"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    // Drei eigene Apps, je eine pro Klingel. P1 behaelt die bisherige
    // Paket-ID, damit vorhandene Installationen aktualisiert werden.
    flavorDimensions += "klingel"
    productFlavors {
        create("p1") {
            dimension = "klingel"
            buildConfigField("int", "BELL", "1")
            resValue("string", "app_name", "Open Intercom EG")
        }
        create("p2") {
            dimension = "klingel"
            applicationIdSuffix = ".p2"
            buildConfigField("int", "BELL", "2")
            resValue("string", "app_name", "Open Intercom 1. Stock")
        }
        create("p3") {
            dimension = "klingel"
            applicationIdSuffix = ".p3"
            buildConfigField("int", "BELL", "3")
            resValue("string", "app_name", "Open Intercom Keller")
        }
    }

    buildFeatures {
        buildConfig = true
        resValues = true
    }

    buildTypes {
        release {
            optimization {
                enable = true
                packageScope = setOf("androidx.**", "kotlin.**", "kotlinx.**")
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

dependencies {
    implementation(libs.androidx.activity.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.core.ktx)
    implementation(libs.material)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(libs.androidx.junit)
}