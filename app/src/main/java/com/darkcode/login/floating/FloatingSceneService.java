package com.darkcode.login.floating;

import android.app.*;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.pm.ServiceInfo;
import android.graphics.PixelFormat;
import android.os.*;
import android.provider.Settings;
import android.view.*;
import android.widget.*;
import com.darkcode.login.scene.ScenePanel;
import com.darkcode.login.ui.Palette;
import com.darkcode.login.ui.SceneActivity;

public final class FloatingSceneService extends Service {
    public static final String STOP="com.darkcode.login.CLOSE_SCENE", MINIMIZE="com.darkcode.login.MINIMIZE_SCENE", EXPAND="com.darkcode.login.EXPAND_SCENE";
    public static volatile boolean running, minimized;
    private WindowManager windows;
    private WindowManager.LayoutParams layout;
    private LinearLayout root;
    private ScenePanel panel;
    private ScrollView content;
    private Button minimize, close;
    private TextView drag;
    private boolean attached;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final Runnable permissionCheck=new Runnable() {
        public void run() {if(!Settings.canDrawOverlays(FloatingSceneService.this)){stopSelf();return;}handler.postDelayed(this,1000);}
    };
    @Override public IBinder onBind(Intent intent) {return null;}
    @Override public void onCreate() {super.onCreate();windows=(WindowManager)getSystemService(WINDOW_SERVICE);}
    @Override public int onStartCommand(Intent intent,int flags,int id) {
        String action=intent==null?null:intent.getAction();
        if(STOP.equals(action)){stopSelf();return START_NOT_STICKY;}
        if(!Settings.canDrawOverlays(this)){stopSelf();return START_NOT_STICKY;}
        try {
            showNotification();
            if(root==null)createWindow();
            if(MINIMIZE.equals(action))setMinimized(true);
            else if(EXPAND.equals(action))setMinimized(false);
            running=true;handler.removeCallbacks(permissionCheck);handler.post(permissionCheck);
        }catch(RuntimeException e){Toast.makeText(this,"No se pudo abrir la ventana flotante",Toast.LENGTH_LONG).show();stopSelf();}
        return START_NOT_STICKY;
    }
    private void showNotification() {
        NotificationManager manager=getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel("scene","Ventana de prueba",NotificationManager.IMPORTANCE_LOW));
        PendingIntent open=PendingIntent.getActivity(this,0,new Intent(this,SceneActivity.class),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        PendingIntent stop=PendingIntent.getService(this,1,new Intent(this,FloatingSceneService.class).setAction(STOP),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        Notification notification=new Notification.Builder(this,"scene").setSmallIcon(android.R.drawable.ic_menu_view)
            .setContentTitle("Dark Code · escena de prueba").setContentText("Ventana flotante activa")
            .setContentIntent(open).setOngoing(true).setOnlyAlertOnce(true)
            .addAction(new Notification.Action.Builder(null,"Cerrar",stop).build()).build();
        if(Build.VERSION.SDK_INT>=34)startForeground(10,notification,ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else startForeground(10,notification);
    }
    private void createWindow() {
        minimized=false;root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);
        root.setBackground(Palette.background(this,Palette.DARK,18));root.setClipToOutline(true);
        LinearLayout header=new LinearLayout(this);header.setGravity(Gravity.CENTER_VERTICAL);
        drag=Palette.text(this,"  DARK CODE",14,Palette.GOLD,true);drag.setContentDescription("Arrastrar ventana; tocar para restaurar si está minimizada");
        drag.setGravity(Gravity.CENTER_VERTICAL);
        header.addView(drag,new LinearLayout.LayoutParams(0,dp(52),1));
        minimize=Palette.button(this,"−",false);minimize.setContentDescription("Minimizar ventana");minimize.setOnClickListener(v->setMinimized(true));
        close=Palette.button(this,"×",false);close.setContentDescription("Cerrar ventana");close.setOnClickListener(v->stopSelf());
        header.addView(minimize,new LinearLayout.LayoutParams(dp(48),dp(48)));header.addView(close,new LinearLayout.LayoutParams(dp(48),dp(48)));
        root.addView(header,new LinearLayout.LayoutParams(-1,dp(52)));
        content=new ScrollView(this);panel=new ScenePanel(this,true);content.addView(panel);root.addView(content,new LinearLayout.LayoutParams(-1,0,1));
        layout=new WindowManager.LayoutParams(dp(360),dp(490),WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE|WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,PixelFormat.TRANSLUCENT);
        layout.gravity=Gravity.TOP|Gravity.LEFT;layout.x=dp(12);layout.y=dp(50);fit();
        drag.setOnClickListener(v->{if(minimized)setMinimized(false);});
        drag.setOnTouchListener(new View.OnTouchListener() {
            float x,y;int startX,startY;boolean moved;
            public boolean onTouch(View v,MotionEvent event) {
                switch(event.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:x=event.getRawX();y=event.getRawY();startX=layout.x;startY=layout.y;moved=false;return true;
                    case MotionEvent.ACTION_MOVE:
                        float dx=event.getRawX()-x,dy=event.getRawY()-y;
                        if(Math.abs(dx)+Math.abs(dy)>dp(4))moved=true;
                        layout.x=startX+Math.round(dx);layout.y=startY+Math.round(dy);fit();update();return true;
                    case MotionEvent.ACTION_UP:if(!moved)v.performClick();return true;
                    case MotionEvent.ACTION_CANCEL:return true;
                    default:return false;
                }
            }
        });
        windows.addView(root,layout);attached=true;
    }
    private void setMinimized(boolean value) {
        if(root==null||value==minimized)return;minimized=value;
        if(value) {panel.pause();content.setVisibility(View.GONE);minimize.setVisibility(View.GONE);close.setVisibility(View.GONE);drag.setText("  DC  +");}
        else {content.setVisibility(View.VISIBLE);minimize.setVisibility(View.VISIBLE);close.setVisibility(View.VISIBLE);drag.setText("  DARK CODE");panel.resume();}
        fit();update();
    }
    private void fit() {
        android.graphics.Rect bounds;
        if(Build.VERSION.SDK_INT>=30)bounds=windows.getCurrentWindowMetrics().getBounds();
        else {android.util.DisplayMetrics metrics=new android.util.DisplayMetrics();windows.getDefaultDisplay().getMetrics(metrics);bounds=new android.graphics.Rect(0,0,metrics.widthPixels,metrics.heightPixels);}
        int availableWidth=Math.max(dp(120),bounds.width()-dp(16));
        int availableHeight=Math.max(dp(100),bounds.height()-dp(80));
        layout.width=minimized?dp(84):Math.min(dp(360),availableWidth);
        layout.height=minimized?dp(52):Math.min(dp(490),availableHeight);
        layout.x=Math.max(0,Math.min(layout.x,availableWidth-layout.width));
        layout.y=Math.max(0,Math.min(layout.y,availableHeight-layout.height));
    }
    private void update() {if(attached)try{windows.updateViewLayout(root,layout);}catch(RuntimeException e){stopSelf();}}
    @Override public void onConfigurationChanged(Configuration config) {super.onConfigurationChanged(config);if(layout!=null){fit();update();}}
    @Override public void onDestroy() {
        handler.removeCallbacksAndMessages(null);running=false;minimized=false;
        if(panel!=null)panel.release();
        if(attached)try{windows.removeViewImmediate(root);}catch(IllegalArgumentException ignored){}
        attached=false;root=null;panel=null;stopForeground(STOP_FOREGROUND_REMOVE);super.onDestroy();
    }
    private int dp(int value) {return Palette.dp(this,value);}
}
