package com.darkcode.login;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.Application;
import android.os.Bundle;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

/** Closes owned screens and tasks through Android's lifecycle. */
public final class DarkCodeApplication extends Application {
    private final Set<Activity> activities = new HashSet<>();

    @Override public void onCreate() {
        super.onCreate();
        registerActivityLifecycleCallbacks(new ActivityLifecycleCallbacks() {
            @Override public void onActivityCreated(Activity activity, Bundle state) { activities.add(activity); }
            @Override public void onActivityDestroyed(Activity activity) { activities.remove(activity); }
            @Override public void onActivityStarted(Activity activity) { }
            @Override public void onActivityResumed(Activity activity) { }
            @Override public void onActivityPaused(Activity activity) { }
            @Override public void onActivityStopped(Activity activity) { }
            @Override public void onActivitySaveInstanceState(Activity activity, Bundle state) { }
        });
    }

    public void closeApp() {
        AccountState.clear();
        for (Activity activity : new ArrayList<>(activities)) activity.finishAndRemoveTask();
        ActivityManager manager = getSystemService(ActivityManager.class);
        if (manager != null) {
            for (ActivityManager.AppTask task : manager.getAppTasks()) task.finishAndRemoveTask();
        }
    }
}
