package com.darkcode.login.ui;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.widget.*;
import com.darkcode.login.floating.FloatingSceneService;
import com.darkcode.login.scene.ScenePanel;

public final class SceneActivity extends Activity {
    private ScenePanel panel;
    private TextView explanation;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setBackgroundColor(Palette.CREAM);
        scroll.setOnApplyWindowInsetsListener((v,insets)-> {v.setPadding(insets.getSystemWindowInsetLeft(),insets.getSystemWindowInsetTop(),insets.getSystemWindowInsetRight(),insets.getSystemWindowInsetBottom());return insets;});
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);
        int p=Palette.dp(this,20);root.setPadding(p,p,p,p);scroll.addView(root);
        LinearLayout header=new LinearLayout(this);header.setGravity(android.view.Gravity.CENTER_VERTICAL);
        Button back=Palette.button(this,"‹ Volver",false);back.setOnClickListener(v->finish());
        header.addView(back,new LinearLayout.LayoutParams(Palette.dp(this,94),Palette.dp(this,48)));
        TextView title=Palette.text(this,"  DARK CODE",19,Palette.DARK,true);header.addView(title);
        root.addView(header);
        TextView intro=Palette.text(this,"Tu espacio de prueba",26,Palette.DARK,true);intro.setPadding(0,p,0,p);root.addView(intro);
        panel=new ScenePanel(this,false);root.addView(panel,new LinearLayout.LayoutParams(-1,-2));
        explanation=Palette.text(this,"",14,Palette.DARK,false);explanation.setPadding(0,p,0,p);root.addView(explanation);
        Button floating=Palette.button(this,"Abrir ventana flotante",true);
        floating.setOnClickListener(v->openFloating());root.addView(floating,new LinearLayout.LayoutParams(-1,Palette.dp(this,56)));
        Button close=Palette.button(this,"Cerrar ventana flotante",false);
        close.setOnClickListener(v-> {stopService(new Intent(this,FloatingSceneService.class));explanation.setText("Ventana cerrada.");});
        LinearLayout.LayoutParams closeParams=new LinearLayout.LayoutParams(-1,Palette.dp(this,48));closeParams.topMargin=Palette.dp(this,10);root.addView(close,closeParams);
        setContentView(scroll);
    }
    private void openFloating() {
        if(!Settings.canDrawOverlays(this)) {
            explanation.setText("Activa «Mostrar sobre otras apps» y vuelve aquí para abrir la ventana.");
            try {startActivity(new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,Uri.parse("package:"+getPackageName())));}
            catch(android.content.ActivityNotFoundException e) {explanation.setText("Abre Ajustes → Apps → Acceso especial → Mostrar sobre otras apps.");}
            return;
        }
        if(Build.VERSION.SDK_INT>=33 && checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS)!=android.content.pm.PackageManager.PERMISSION_GRANTED)
            requestPermissions(new String[]{android.Manifest.permission.POST_NOTIFICATIONS},14);
        try {
            startForegroundService(new Intent(this,FloatingSceneService.class));
            explanation.setText("Arrastra la barra superior. Usa − para minimizar y × para cerrar.");
        } catch(RuntimeException e) {explanation.setText("Android no pudo abrir la ventana. Vuelve a intentarlo desde esta pantalla.");}
    }
    @Override protected void onResume() {super.onResume();if(panel!=null)panel.resume();
        if(explanation!=null)explanation.setText(Settings.canDrawOverlays(this)?"Ventana flotante disponible.":"La ventana flotante necesita permiso para mostrarse sobre otras apps.");}
    @Override protected void onPause() {if(panel!=null)panel.pause();super.onPause();}
    @Override protected void onDestroy() {if(panel!=null)panel.release();super.onDestroy();}
}
