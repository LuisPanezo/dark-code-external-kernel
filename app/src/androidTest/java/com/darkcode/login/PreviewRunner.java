package com.darkcode.login;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import com.darkcode.login.scene.NativeBridge;
import com.darkcode.login.scene.ScenePanel;
import com.darkcode.login.ui.SceneActivity;
import com.darkcode.login.floating.FloatingSceneService;

/** Screenshot harness installed only in the separate test APK. No authentication bypass. */
public final class PreviewRunner extends Instrumentation {
    private volatile Activity resumedScene;
    @Override public void callActivityOnResume(Activity activity) {
        super.callActivityOnResume(activity);
        if(activity instanceof SceneActivity)resumedScene=activity;
    }
    @Override public void onCreate(Bundle arguments) { super.onCreate(arguments); start(); }
    private Object field(Activity activity, String name) throws Exception {
        Field field = activity.getClass().getDeclaredField(name);
        field.setAccessible(true);
        return field.get(activity);
    }
    private void capture(String name) throws Exception {
        waitForIdleSync();
        Thread.sleep(500); // Allow rendered frames and elevation shadows to settle.
        Bitmap bitmap = getUiAutomation().takeScreenshot();
        if (bitmap == null) throw new IllegalStateException("Screenshot unavailable");
        File directory = getTargetContext().getExternalFilesDir(null);
        if (directory == null) throw new IllegalStateException("External app files unavailable");
        File file = new File(directory, name + ".png");
        try (FileOutputStream output = new FileOutputStream(file)) {
            if (!bitmap.compress(Bitmap.CompressFormat.PNG, 100, output)) throw new IllegalStateException("Screenshot failed");
        } finally { bitmap.recycle(); }
    }
    @Override public void onStart() {
        Activity activity = null;
        Bundle result = new Bundle();
        try {
            getUiAutomation().setRotation(android.view.Surface.ROTATION_0);
            Thread.sleep(500);
            activity = startActivitySync(new Intent(getTargetContext(), LoginActivity.class)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK));
            capture("01-login");
            View submit = (View) field(activity, "signIn");
            runOnMainSync(submit::performClick);
            capture("02-validation");
            TextView status = (TextView) field(activity, "status");
            if (!status.getText().toString().equals("Escribe tu usuario."))
                throw new AssertionError("Empty login was not rejected");
            View login = (View) field(activity, "loginCard");
            View account = (View) field(activity, "accountCard");
            TextView accountStatus = (TextView) field(activity, "accountStatus");
            runOnMainSync(() -> {
                login.setVisibility(View.GONE);
                account.setVisibility(View.VISIBLE);
                accountStatus.setText("Vista previa · usuario_demo\nSesión no iniciada.");
            });
            capture("03-account-preview");
            runOnMainSync(() -> { account.setVisibility(View.GONE); login.setVisibility(View.VISIBLE); status.setVisibility(View.GONE); });
            String nativeResult=NativeBridge.runCoreTests();
            if(nativeResult.startsWith("FAILED"))throw new AssertionError(nativeResult);
            Activity demo=startActivitySync(new Intent(getTargetContext(),SceneActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
            ScenePanel panel=(ScenePanel)field(demo,"panel");
            awaitScene(panel);
            capture("04-scene");
            for(int source=1;source<=3;++source) {
                final int selected=source;
                runOnMainSync(()->panel.selectSource(selected));
                Thread.sleep(150);awaitScene(panel);
            }
            runOnMainSync(()-> {panel.selectSource(2);panel.selectTab(1);});
            capture("05-settings");
            runOnMainSync(()-> {panel.scene.setPaused(true);panel.selectTab(2);});
            for(int i=0;i<40&&!panel.scene.latestStatus().equals("Pausado");++i)Thread.sleep(100);
            if(!panel.scene.latestStatus().equals("Pausado"))throw new AssertionError("Pause did not take effect");
            capture("06-state");
            runOnMainSync(()->panel.scene.setPaused(false));
            // The floating account panel must reject starts without an authenticated account.
            getTargetContext().startForegroundService(new Intent(getTargetContext(),FloatingSceneService.class));
            Thread.sleep(500);
            if(FloatingSceneService.running)throw new AssertionError("Unauthenticated overlay was allowed");
            runOnMainSync(()->demo.setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE));
            Thread.sleep(1200);
            Activity landscape=resumedScene;
            awaitScene((ScenePanel)field(landscape,"panel"));capture("10-landscape");
            if(landscape.getResources().getConfiguration().orientation!=android.content.res.Configuration.ORIENTATION_LANDSCAPE)
                throw new AssertionError("Landscape configuration missing");
            runOnMainSync(()->landscape.setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED));
            result.putString("stream", nativeResult+". Login validation and four scene sources passed. Pause/resume and rotation passed. Floating checks: "+"unauthenticated start rejected"+".\n");
            finish(Activity.RESULT_OK, result);
        } catch (Exception | AssertionError e) {
            getTargetContext().stopService(new Intent(getTargetContext(),FloatingSceneService.class));
            result.putString("stream", "Preview capture failed: " + e.getClass().getSimpleName() + ": "+e.getMessage()+"\n");
            finish(Activity.RESULT_CANCELED, result);
        } finally {getUiAutomation().setRotation(android.app.UiAutomation.ROTATION_UNFREEZE);}
    }
    private void awaitScene(ScenePanel panel) throws Exception {
        for(int i=0;i<50&&panel.scene.entityCount()!=3;++i)Thread.sleep(100);
        if(panel.scene.entityCount()!=3)throw new AssertionError("Scene failed: "+panel.scene.latestStatus());
    }
}
