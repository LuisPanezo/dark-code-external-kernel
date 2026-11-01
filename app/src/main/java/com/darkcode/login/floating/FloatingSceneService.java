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
import com.darkcode.login.AccountState;
import com.darkcode.login.LoginActivity;
import com.darkcode.login.ui.Palette;
import com.darkcode.login.DarkCodeApplication;

public final class FloatingSceneService extends Service {
    public static final String STOP="com.darkcode.login.CLOSE_SCENE", MINIMIZE="com.darkcode.login.MINIMIZE_SCENE", EXPAND="com.darkcode.login.EXPAND_SCENE";
    public static final String EXIT_APP="com.darkcode.login.EXIT_APP";
    public static volatile boolean running, minimized;
    private WindowManager windows;
    private WindowManager.LayoutParams layout;
    private LinearLayout root;
    private FloatingPanel content;
    private Button minimize, close;
    private TextView drag;
    private boolean attached;
    private LinearLayout resizeBar;
    private int desiredWidth, desiredHeight;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final Runnable permissionCheck=new Runnable() {
        public void run() {if(!Settings.canDrawOverlays(FloatingSceneService.this)||!AccountState.canOpenPanel()){stopSelf();return;}handler.postDelayed(this,1000);}
    };
    @Override public IBinder onBind(Intent intent) {return null;}
    @Override public void onCreate() {super.onCreate();windows=(WindowManager)getSystemService(WINDOW_SERVICE);}
    @Override public int onStartCommand(Intent intent,int flags,int id) {
        String action=intent==null?null:intent.getAction();
        if(EXIT_APP.equals(action)){closeApp();return START_NOT_STICKY;}
        if(STOP.equals(action)){stopSelf();return START_NOT_STICKY;}
        if(!AccountState.canOpenPanel()){reportReady(intent, false);stopSelf();return START_NOT_STICKY;}
        if(!Settings.canDrawOverlays(this)){reportReady(intent, false);stopSelf();return START_NOT_STICKY;}
        try {
            showNotification();
            if(root==null)createWindow();
            if(MINIMIZE.equals(action))setMinimized(true);
            else if(EXPAND.equals(action))setMinimized(false);
            running=true;handler.removeCallbacks(permissionCheck);handler.post(permissionCheck);
            reportReady(intent, attached);
        }catch(RuntimeException e){reportReady(intent, false);Toast.makeText(this,"No se pudo abrir la ventana flotante",Toast.LENGTH_LONG).show();stopSelf();}
        return START_NOT_STICKY;
    }
    private void reportReady(Intent intent, boolean ready) {
        if (intent == null) return;
        ResultReceiver receiver = intent.getParcelableExtra("panel_ready");
        if (receiver != null) receiver.send(ready ? 1 : 0, Bundle.EMPTY);
    }
    private void showNotification() {
        NotificationManager manager=getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel("scene","Panel flotante",NotificationManager.IMPORTANCE_LOW));
        PendingIntent open=PendingIntent.getActivity(this,0,new Intent(this,LoginActivity.class),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        PendingIntent stop=PendingIntent.getService(this,1,new Intent(this,FloatingSceneService.class).setAction(EXIT_APP),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        Notification notification=new Notification.Builder(this,"scene").setSmallIcon(android.R.drawable.ic_menu_view)
            .setContentTitle("Dark Code · tu espacio").setContentText("Ventana flotante activa")
            .setContentIntent(open).setOngoing(true).setOnlyAlertOnce(true)
            .addAction(new Notification.Action.Builder(null,"Salir de Dark Code",stop).build()).build();
        if(Build.VERSION.SDK_INT>=34)startForeground(10,notification,ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else startForeground(10,notification);
    }
    private void createWindow() {
        minimized=false;root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);
        root.setBackground(panelBackground());root.setClipToOutline(true);
        LinearLayout header=new LinearLayout(this);header.setGravity(Gravity.CENTER_VERTICAL);
        header.setPadding(dp(12),0,dp(4),0);header.setBackgroundColor(0xFF0E1015);
        drag=Palette.text(this,"DARK CODE",12,0xFFF3F4F7,true);drag.setContentDescription("Arrastrar ventana; tocar para restaurar si está minimizada");
        drag.setLetterSpacing(.12f);drag.setSingleLine(true);drag.setEllipsize(android.text.TextUtils.TruncateAt.END);
        drag.setGravity(Gravity.CENTER_VERTICAL);
        header.addView(drag,new LinearLayout.LayoutParams(0,dp(52),1));
        minimize=headerButton("−");minimize.setContentDescription("Minimizar ventana");minimize.setOnClickListener(v->setMinimized(true));
        close=headerButton("×");close.setContentDescription("Cerrar Dark Code por completo");close.setTooltipText("Salir de Dark Code");close.setTextColor(0xFFFF667A);close.setOnClickListener(v->closeApp());
        header.addView(minimize,new LinearLayout.LayoutParams(dp(48),dp(48)));header.addView(close,new LinearLayout.LayoutParams(dp(48),dp(48)));
        root.addView(header,new LinearLayout.LayoutParams(-1,dp(52)));
        View headerRule=new View(this);headerRule.setBackgroundColor(0xFF242730);root.addView(headerRule,new LinearLayout.LayoutParams(-1,dp(1)));
        content=new FloatingPanel(this,opacity->{layout.alpha=opacity;update();});
        root.addView(content,new LinearLayout.LayoutParams(-1,0,1));
        resizeBar=new LinearLayout(this);resizeBar.setGravity(Gravity.RIGHT|Gravity.CENTER_VERTICAL);
        resizeBar.setBackgroundColor(0xFF0E1015);
        Button resize=headerButton("◢");resize.setTextSize(16);
        resize.setContentDescription("Cambiar tamaño: arrastrar; tocar alterna tamaño compacto y grande");
        resize.setTooltipText("Arrastra para cambiar el tamaño");
        resizeBar.addView(resize,new LinearLayout.LayoutParams(dp(44),dp(44)));
        root.addView(resizeBar,new LinearLayout.LayoutParams(-1,dp(44)));
        android.content.SharedPreferences sizePrefs=getSharedPreferences("panel_size",MODE_PRIVATE);
        desiredWidth=dp(sizePrefs.getInt("width",540));desiredHeight=dp(sizePrefs.getInt("height",580));
        resize.setOnClickListener(v->{
            boolean compact=desiredWidth>dp(420);
            desiredWidth=dp(compact?360:620);desiredHeight=dp(compact?460:620);
            fit();update();saveSize();
        });
        resize.setOnTouchListener(new View.OnTouchListener(){
            float x,y;int width,height;boolean moved;
            public boolean onTouch(View view,MotionEvent event){
                switch(event.getActionMasked()){
                    case MotionEvent.ACTION_DOWN:
                        x=event.getRawX();y=event.getRawY();width=layout.width;height=layout.height;moved=false;return true;
                    case MotionEvent.ACTION_MOVE:
                        float dx=event.getRawX()-x,dy=event.getRawY()-y;
                        if(Math.abs(dx)+Math.abs(dy)>dp(4))moved=true;
                        desiredWidth=Math.max(dp(320),width+Math.round(dx));
                        desiredHeight=Math.max(dp(360),height+Math.round(dy));
                        fit();update();return true;
                    case MotionEvent.ACTION_UP:
                        if(moved){desiredWidth=layout.width;desiredHeight=layout.height;saveSize();}
                        else view.performClick();return true;
                    case MotionEvent.ACTION_CANCEL:return true;
                    default:return false;
                }
            }
        });
        layout=new WindowManager.LayoutParams(dp(620),dp(620),WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE|WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,PixelFormat.TRANSLUCENT);
        layout.alpha=getSharedPreferences("panel_appearance",MODE_PRIVATE).getInt("opacity",100)/100f;
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
        if(value) {content.setVisibility(View.GONE);minimize.setVisibility(View.GONE);close.setVisibility(View.VISIBLE);}
        else {content.setVisibility(View.VISIBLE);minimize.setVisibility(View.VISIBLE);close.setVisibility(View.VISIBLE);}
        resizeBar.setVisibility(value?View.GONE:View.VISIBLE);
        fit();update();
    }
    private void saveSize(){
        float density=getResources().getDisplayMetrics().density;
        getSharedPreferences("panel_size",MODE_PRIVATE).edit()
            .putInt("width",Math.round(layout.width/density)).putInt("height",Math.round(layout.height/density)).apply();
    }
    private void fit() {
        android.graphics.Rect bounds;
        if(Build.VERSION.SDK_INT>=30)bounds=windows.getCurrentWindowMetrics().getBounds();
        else {android.util.DisplayMetrics metrics=new android.util.DisplayMetrics();windows.getDefaultDisplay().getMetrics(metrics);bounds=new android.graphics.Rect(0,0,metrics.widthPixels,metrics.heightPixels);}
        int availableWidth=Math.max(dp(120),bounds.width()-dp(16));
        int availableHeight=Math.max(dp(100),bounds.height()-dp(80));
        layout.width=minimized?Math.min(dp(180),availableWidth):Math.min(Math.max(dp(320),desiredWidth),availableWidth);
        layout.height=minimized?dp(52):Math.min(Math.max(dp(360),desiredHeight),availableHeight);
        layout.x=Math.max(0,Math.min(layout.x,availableWidth-layout.width));
        layout.y=Math.max(0,Math.min(layout.y,availableHeight-layout.height));
    }
    private void update() {if(attached)try{windows.updateViewLayout(root,layout);}catch(RuntimeException e){stopSelf();}}
    @Override public void onConfigurationChanged(Configuration config) {super.onConfigurationChanged(config);if(layout!=null){fit();update();}}
    private void closeApp() {
        removeWindow();
        stopSelf();
        ((DarkCodeApplication)getApplication()).closeApp();
    }
    private void removeWindow() {
        handler.removeCallbacksAndMessages(null);running=false;minimized=false;
        if(attached)try{windows.removeViewImmediate(root);}catch(IllegalArgumentException ignored){}
        attached=false;root=null;content=null;stopForeground(STOP_FOREGROUND_REMOVE);
    }
    @Override public void onDestroy() {
        removeWindow();
        super.onDestroy();
    }
    private android.graphics.drawable.GradientDrawable panelBackground() {
        android.graphics.drawable.GradientDrawable d=new android.graphics.drawable.GradientDrawable();
        d.setColor(0xFF090A0D);d.setCornerRadius(dp(20));d.setStroke(dp(1),0xFF30333D);return d;
    }
    private Button headerButton(String label) {
        Button b=new Button(this);b.setText(label);b.setTextColor(0xFFADB2BF);b.setTextSize(22);
        b.setMinWidth(0);b.setMinimumWidth(0);b.setPadding(0,0,0,0);b.setStateListAnimator(null);
        android.graphics.drawable.GradientDrawable surface=new android.graphics.drawable.GradientDrawable();
        surface.setColor(0xFF0E1015);surface.setCornerRadius(dp(10));
        b.setBackground(new android.graphics.drawable.RippleDrawable(android.content.res.ColorStateList.valueOf(0x33FFFFFF),surface,null));return b;
    }
    private int dp(int value) {return Palette.dp(this,value);}
}
