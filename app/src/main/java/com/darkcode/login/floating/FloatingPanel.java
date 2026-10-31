package com.darkcode.login.floating;

import android.content.*;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.net.Uri;
import android.view.Gravity;
import android.view.View;
import android.widget.*;
import com.darkcode.login.AccountState;
import com.darkcode.login.R;
import com.darkcode.login.esp.EspOverlayService;

/** Floating navigation, account and appearance controls. */
public final class FloatingPanel extends LinearLayout {
    public interface Appearance { void apply(float opacity); }
    private static final int BG=0xFF090A0D, CARD=0xFF111318, BORDER=0xFF242730,
            TEXT=0xFFF3F4F7, MUTED=0xFFA3A9B5;
    private final SharedPreferences prefs;
    private final Appearance appearance;
    private int accent, page;
    private LinearLayout body, sidebar;
    public FloatingPanel(Context context, Appearance appearance) {
        super(context); this.appearance=appearance;
        prefs=context.getSharedPreferences("panel_appearance",Context.MODE_PRIVATE);
        accent=prefs.getInt("accent",0xFFFF304F);
        setOrientation(HORIZONTAL); setBackgroundColor(BG); build();
    }
    private void build() {
        removeAllViews();
        sidebar=new LinearLayout(getContext());sidebar.setOrientation(VERTICAL);
        sidebar.setBackgroundColor(0xFF060708);
        addView(sidebar,new LinearLayout.LayoutParams(dp(132),-1));
        View divider=new View(getContext());divider.setBackgroundColor(BORDER);
        addView(divider,new LinearLayout.LayoutParams(dp(1),-1));
        addBrand();
        ScrollView navigation=new ScrollView(getContext());navigation.setFillViewport(true);
        navigation.setVerticalScrollBarEnabled(false);
        LinearLayout links=new LinearLayout(getContext());links.setOrientation(VERTICAL);links.setPadding(dp(6),dp(10),dp(6),dp(10));
        String[] labels={"AIM","VISUAL","Ventana","Cuenta","Soporte","Ajustes"};
        int[] icons={R.drawable.panel_home,R.drawable.panel_visual,R.drawable.panel_window,
                R.drawable.panel_account,R.drawable.panel_support,R.drawable.panel_settings};
        for(int i=0;i<labels.length;i++) {
            final int index=i;
            LinearLayout item=new LinearLayout(getContext());item.setGravity(Gravity.CENTER_VERTICAL);
            item.setBackground(interactiveSurface(page==i?0xFF171A20:0xFF060708,10,page==i?BORDER:0xFF060708));
            View marker=new View(getContext());marker.setBackground(shape(page==i?controlAccent():android.graphics.Color.TRANSPARENT,2,android.graphics.Color.TRANSPARENT));
            item.addView(marker,new LinearLayout.LayoutParams(dp(3),dp(22)));
            ImageView icon=new ImageView(getContext());icon.setImageResource(icons[i]);icon.setColorFilter(page==i?controlAccent():MUTED);
            icon.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            LinearLayout.LayoutParams iconParams=new LinearLayout.LayoutParams(dp(18),dp(18));iconParams.leftMargin=dp(8);iconParams.rightMargin=dp(8);
            item.addView(icon,iconParams);
            TextView label=text(labels[i],12,page==i?TEXT:MUTED,page==i);label.setMaxLines(2);
            item.addView(label,new LinearLayout.LayoutParams(0,-2,1));
            item.setContentDescription(labels[i]+(page==i?", seleccionado":""));item.setSelected(page==i);
            item.setFocusable(true);item.setOnClickListener(v->{feedback(v);page=index;build();});
            LinearLayout.LayoutParams itemParams=new LinearLayout.LayoutParams(-1,dp(48));itemParams.bottomMargin=dp(4);
            links.addView(item,itemParams);
        }
        navigation.addView(links);sidebar.addView(navigation,new LinearLayout.LayoutParams(-1,0,1));
        profile();
        ScrollView scroll=new ScrollView(getContext());scroll.setFillViewport(true);scroll.setVerticalScrollBarEnabled(false);
        body=new LinearLayout(getContext());body.setOrientation(VERTICAL);body.setPadding(dp(16),dp(20),dp(16),dp(16));
        scroll.addView(body);addView(scroll,new LinearLayout.LayoutParams(0,-1,1));
        if(page==0)home();else if(page==1)visual();else if(page==2)windowSettings();
        else if(page==3)account();else if(page==4)support();else settings();
    }
    private void addBrand() {
        LinearLayout brand=new LinearLayout(getContext());brand.setOrientation(VERTICAL);
        brand.setGravity(Gravity.CENTER);brand.setPadding(dp(10),dp(16),dp(10),dp(16));
        ImageView phoenix=new ImageView(getContext());phoenix.setImageResource(R.drawable.phoenix);
        phoenix.setContentDescription("Fénix DARK CODE");phoenix.setScaleType(ImageView.ScaleType.FIT_CENTER);
        phoenix.setBackground(shape(0xFFF5F2EC,14,0xFFF5F2EC));phoenix.setClipToOutline(true);
        brand.addView(phoenix,new LinearLayout.LayoutParams(dp(52),dp(52)));gap(brand,12);
        LinearLayout wordmark=new LinearLayout(getContext());wordmark.setGravity(Gravity.CENTER);
        TextView dark=text("DARK ",12,TEXT,true);dark.setTextSize(12);dark.setLetterSpacing(.12f);
        TextView code=text("CODE",12,controlAccent(),true);code.setTextSize(12);code.setLetterSpacing(.12f);
        wordmark.addView(dark);wordmark.addView(code);brand.addView(wordmark);
        sidebar.addView(brand,new LinearLayout.LayoutParams(-1,-2));
        View rule=new View(getContext());rule.setBackgroundColor(BORDER);sidebar.addView(rule,new LinearLayout.LayoutParams(-1,dp(1)));
    }
    private void home() {
        heading("DARK CODE","Hola, "+AccountState.username()+".");
        LinearLayout welcome=card("A TU MEDIDA");
        welcome.addView(text("Tu panel. Tu estilo.",18,TEXT,true));gap(welcome,16);
        Button customize=button("Personalizar",true);
        customize.setOnClickListener(v->{page=5;build();});welcome.addView(customize,new LinearLayout.LayoutParams(-1,dp(52)));
        LinearLayout account=card("TU SUSCRIPCIÓN");
        account.addView(text(AccountState.plan(),13,TEXT,false));gap(account,8);
        account.addView(text(AccountState.remaining(),12,MUTED,false));
        Button details=button("Ver mi cuenta",false);details.setOnClickListener(v->{page=3;build();});
        gap(account,12);account.addView(details,new LinearLayout.LayoutParams(-1,dp(48)));
    }
    private void settings() {
        heading("Ajustes","");
        colorSettings();
        LinearLayout preferences=card("PREFERENCIAS");
        toggle(preferences,"Respuesta táctil","","haptics",true);
        toggle(preferences,"Días restantes","En el perfil","show_days",true);
        Button reset=button("Restablecer apariencia",false);
        reset.setOnClickListener(v->{prefs.edit().clear().apply();accent=0xFFFF304F;appearance.apply(1f);build();});
        body.addView(reset,new LinearLayout.LayoutParams(-1,dp(52)));
    }
    private void heading(String title,String subtitle) {
        TextView titleView=text(title,20,TEXT,true);if(android.os.Build.VERSION.SDK_INT>=28)titleView.setAccessibilityHeading(true);body.addView(titleView);
        if(!subtitle.isEmpty()) {TextView intro=text(subtitle,12,MUTED,false);intro.setPadding(0,dp(6),0,0);body.addView(intro);}
        gap(body,20);
    }
    private void account() {
        heading("Tu cuenta","");
        LinearLayout details=card("PERFIL");
        details.addView(text(AccountState.username(),16,TEXT,true));gap(details,12);
        details.addView(text(AccountState.plan(),13,MUTED,false));gap(details,8);
        details.addView(text(AccountState.remaining(),13,controlAccent(),false));
        toggle(details,"Días restantes","En el perfil","show_days",true);
    }
    private void visual() {
        body.addView(text("VISUAL",20,TEXT,true));gap(body,16);
        CheckBox option=new CheckBox(getContext());option.setText("ESP");
        option.setTextColor(TEXT);option.setTextSize(14);
        option.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));
        option.setMinHeight(dp(48));
        option.setButtonTintList(new ColorStateList(new int[][]{
                new int[]{android.R.attr.state_checked},new int[]{}},new int[]{controlAccent(),MUTED}));
        android.content.SharedPreferences options=getContext().getSharedPreferences("panel_options",Context.MODE_PRIVATE);
        option.setChecked(options.getBoolean("op2",false) || EspOverlayService.running);
        body.addView(option,new LinearLayout.LayoutParams(-1,-2));
        LinearLayout children=new LinearLayout(getContext());children.setOrientation(VERTICAL);
        children.setPadding(dp(12),dp(6),0,0);
        body.addView(children,new LinearLayout.LayoutParams(-1,-2));
        String[] positions={"Arriba","Abajo","Centro"};
        int selected=Math.max(0,Math.min(positions.length-1,options.getInt("op2_position",2)));
        Button dropdown=button(positions[selected]+"  ▾",false);
        dropdown.setGravity(Gravity.CENTER_VERTICAL|Gravity.START);
        dropdown.setPadding(dp(14),0,dp(14),0);
        dropdown.setContentDescription("Posición: "+positions[selected]+". Abrir opciones");
        children.addView(dropdown,new LinearLayout.LayoutParams(-1,dp(48)));
        RadioGroup choices=new RadioGroup(getContext());choices.setOrientation(VERTICAL);
        choices.setPadding(dp(8),dp(4),dp(8),dp(4));
        choices.setBackground(shape(CARD,10,BORDER));
        LinearLayout.LayoutParams choicesParams=new LinearLayout.LayoutParams(-1,-2);
        choicesParams.topMargin=dp(6);children.addView(choices,choicesParams);
        for(int i=0;i<positions.length;i++) {
            RadioButton choice=new RadioButton(getContext());choice.setId(View.generateViewId());
            choice.setTag(i);choice.setText(positions[i]);choice.setTextSize(14);choice.setTextColor(TEXT);
            choice.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));
            choice.setButtonTintList(new ColorStateList(new int[][]{
                    new int[]{android.R.attr.state_checked},new int[]{}},new int[]{controlAccent(),MUTED}));
            choices.addView(choice,new RadioGroup.LayoutParams(-1,dp(48)));
            if(i==selected)choices.check(choice.getId());
        }
        choices.setVisibility(GONE);
        dropdown.setOnClickListener(v->{
            boolean expand=choices.getVisibility()!=VISIBLE;
            choices.setVisibility(expand?VISIBLE:GONE);
            int position=(Integer)choices.findViewById(choices.getCheckedRadioButtonId()).getTag();
            dropdown.setText(positions[position]+(expand?"  ▴":"  ▾"));
            dropdown.setContentDescription("Posición: "+positions[position]+(expand?". Cerrar opciones":". Abrir opciones"));
        });
        choices.setOnCheckedChangeListener((group,id)->{
            int position=(Integer)group.findViewById(id).getTag();
            options.edit().putInt("op2_position",position).apply();
            dropdown.setText(positions[position]+"  ▾");
            dropdown.setContentDescription("Posición: "+positions[position]+". Abrir opciones");
            choices.setVisibility(GONE);
        });
        espToggle(children,"ESP Box","esp_box",true);
        espToggle(children,"Vida","esp_health",true);
        espToggle(children,"Nombre","esp_name",true);
        espToggle(children,"Distancia","esp_distance",true);
        children.setVisibility(option.isChecked()?VISIBLE:GONE);
        option.setOnCheckedChangeListener((button,checked)->{
            options.edit().putBoolean("op2",checked).apply();
            children.setVisibility(checked?VISIBLE:GONE);choices.setVisibility(GONE);
            int position=(Integer)choices.findViewById(choices.getCheckedRadioButtonId()).getTag();
            dropdown.setText(positions[position]+"  ▾");
            dropdown.setContentDescription("Posición: "+positions[position]+". Abrir opciones");
            setEspOverlay(checked);
        });
    }
    /* Enciende o apaga el overlay ESP (servicio en primer plano). El driver y la
     * lectura del juego los hace el proceso esp_df que lanza EspOverlayView. */
    private void setEspOverlay(boolean enabled) {
        if(com.darkcode.login.BuildConfig.PANEL_ONLY) return;
        Intent intent = new Intent(getContext(), EspOverlayService.class);
        if (enabled) getContext().startForegroundService(intent);
        else getContext().stopService(intent);
    }
    /* Checkbox de opción del ESP: estilo del panel + persistencia en
     * panel_options. Controla qué dibuja el overlay (caja/vida/nombre/distancia). */
    private CheckBox espToggle(LinearLayout parent, String label, String key, boolean fallback) {
        android.content.SharedPreferences options = getContext().getSharedPreferences("panel_options", Context.MODE_PRIVATE);
        CheckBox cb = new CheckBox(getContext());
        cb.setText(label);
        cb.setTextColor(TEXT);
        cb.setTextSize(13);
        cb.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
        cb.setMinHeight(dp(44));
        cb.setButtonTintList(new ColorStateList(new int[][]{
                new int[]{android.R.attr.state_checked}, new int[]{}}, new int[]{controlAccent(), MUTED}));
        cb.setChecked(options.getBoolean(key, fallback));
        cb.setOnCheckedChangeListener((b, checked) -> {
            options.edit().putBoolean(key, checked).apply();
            feedback(b);
        });
        parent.addView(cb, new LinearLayout.LayoutParams(-1, -2));
        return cb;
    }
    private void colorSettings() {
        LinearLayout colors=card("COLOR");
        TextView preview=text(String.format(java.util.Locale.ROOT,"#%06X",accent & 0xFFFFFF),14,TEXT,true);
        preview.setGravity(Gravity.CENTER);preview.setPadding(0,dp(10),0,dp(10));
        preview.setBackground(shape(accent,8,0xFF555B66));
        preview.setTextColor(android.graphics.Color.luminance(accent)>.4?BG:TEXT);
        ColorWheelView wheel=new ColorWheelView(getContext(),accent,color->{
            preview.setText(String.format(java.util.Locale.ROOT,"#%06X",color & 0xFFFFFF));
            preview.setBackground(shape(color,8,0xFF555B66));
            preview.setTextColor(android.graphics.Color.luminance(color)>.4?BG:TEXT);
        });
        LinearLayout.LayoutParams wheelParams=new LinearLayout.LayoutParams(-2,-2);wheelParams.gravity=Gravity.CENTER_HORIZONTAL;
        colors.addView(wheel,wheelParams);gap(colors,6);
        LinearLayout brightnessLabel=new LinearLayout(getContext());
        brightnessLabel.addView(text("Brillo del color",12,MUTED,false),new LinearLayout.LayoutParams(0,-2,1));
        TextView brightnessValue=text(Math.round(wheel.brightness()*100)+"%",12,TEXT,true);
        brightnessLabel.addView(brightnessValue);colors.addView(brightnessLabel);
        SeekBar hue=new SeekBar(getContext());hue.setMax(100);hue.setProgress(Math.round(wheel.brightness()*100));
        hue.setContentDescription("Brillo del color");
        styleSlider(hue);
        hue.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            public void onProgressChanged(SeekBar bar,int value,boolean user){brightnessValue.setText(value+"%");if(user)wheel.setBrightness(value/100f);}
            public void onStartTrackingTouch(SeekBar bar){}
            public void onStopTrackingTouch(SeekBar bar){}
        });
        colors.addView(hue,new LinearLayout.LayoutParams(-1,dp(48)));
        LinearLayout actions=new LinearLayout(getContext());actions.setGravity(Gravity.CENTER_VERTICAL);
        actions.addView(preview,new LinearLayout.LayoutParams(0,dp(44),1));
        Button apply=button("Aplicar",true);
        apply.setOnClickListener(v->{accent=wheel.color();prefs.edit().putInt("accent",accent).apply();build();});
        LinearLayout.LayoutParams applyParams=new LinearLayout.LayoutParams(0,dp(44),1);applyParams.leftMargin=dp(8);
        actions.addView(apply,applyParams);colors.addView(actions);
    }
    private void windowSettings() {
        body.addView(text("Ventana",20,TEXT,true));
    }
    private void support() {
        heading("Soporte","Estamos contigo.");
        LinearLayout channels=card("HABLEMOS");
        channels.addView(text("Elige un canal",14,TEXT,true));gap(channels,16);
        supportButton(channels,"WhatsApp",getContext().getString(R.string.support_whatsapp_url));gap(channels,10);
        supportButton(channels,"Discord",getContext().getString(R.string.support_discord_url));
        TextView privacy=text("Nunca compartas tu contraseña.",12,MUTED,false);privacy.setPadding(0,dp(16),0,0);channels.addView(privacy);
    }
    private void supportButton(LinearLayout parent,String label,String url) {
        Button button=button(url.isEmpty()?label+" · Próximamente":label,true);
        button.setEnabled(!url.isEmpty());
        button.setOnClickListener(v->{
            Uri uri=Uri.parse(url);
            if(!"https".equals(uri.getScheme())) {Toast.makeText(getContext(),"Enlace de soporte no disponible",Toast.LENGTH_SHORT).show();return;}
            try {getContext().startActivity(new Intent(Intent.ACTION_VIEW,uri).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));}
            catch(ActivityNotFoundException e) {Toast.makeText(getContext(),"Instala un navegador para abrir soporte",Toast.LENGTH_SHORT).show();}
        });parent.addView(button,new LinearLayout.LayoutParams(-1,dp(48)));
    }
    private void profile() {
        LinearLayout footer=new LinearLayout(getContext());footer.setOrientation(VERTICAL);
        footer.setPadding(dp(10),dp(14),dp(10),dp(14));footer.setBackground(interactiveSurface(0xFF0A0B0D,0,0xFF0A0B0D));
        View divider=new View(getContext());divider.setBackgroundColor(BORDER);sidebar.addView(divider,new LinearLayout.LayoutParams(-1,dp(1)));
        String username=AccountState.username();
        LinearLayout identity=new LinearLayout(getContext());identity.setGravity(Gravity.CENTER_VERTICAL);
        TextView avatar=text(username.isEmpty()?"?":username.substring(0,username.offsetByCodePoints(0,1)).toUpperCase(java.util.Locale.ROOT),13,controlAccent(),true);
        avatar.setGravity(Gravity.CENTER);avatar.setBackground(shape(0xFF1B1E25,20,BORDER));
        identity.addView(avatar,new LinearLayout.LayoutParams(dp(32),dp(32)));
        TextView name=text(username,12,TEXT,true);name.setSingleLine(true);name.setEllipsize(android.text.TextUtils.TruncateAt.END);name.setPadding(dp(6),0,0,0);
        identity.addView(name,new LinearLayout.LayoutParams(0,-2,1));footer.addView(identity);
        if(prefs.getBoolean("show_days",true)) {gap(footer,8);footer.addView(text(AccountState.remaining(),11,MUTED,false));}
        footer.setContentDescription("Abrir perfil de "+username);footer.setFocusable(true);
        footer.setOnClickListener(v->{page=3;build();});sidebar.addView(footer,new LinearLayout.LayoutParams(-1,-2));
    }
    private LinearLayout card(String title) {
        LinearLayout card=new LinearLayout(getContext());card.setOrientation(VERTICAL);card.setPadding(dp(14),dp(16),dp(14),dp(16));
        card.setBackground(shape(CARD,12,BORDER));
        TextView heading=text(title,11,MUTED,true);heading.setLetterSpacing(.08f);card.addView(heading);gap(card,14);
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.bottomMargin=dp(12);body.addView(card,lp);return card;
    }
    private void styleSlider(SeekBar slider) {
        float[] hsv=new float[3];android.graphics.Color.colorToHSV(accent,hsv);
        hsv[1]=Math.min(hsv[1],.8f);hsv[2]=Math.max(hsv[2],.75f);
        int visibleAccent=android.graphics.Color.HSVToColor(hsv);
        GradientDrawable track=new GradientDrawable();track.setColor(0xFF303640);track.setCornerRadius(dp(3));
        GradientDrawable fill=new GradientDrawable();fill.setColor(visibleAccent);fill.setCornerRadius(dp(3));
        android.graphics.drawable.ClipDrawable progress=new android.graphics.drawable.ClipDrawable(
                fill,Gravity.LEFT,android.graphics.drawable.ClipDrawable.HORIZONTAL);
        android.graphics.drawable.LayerDrawable layers=new android.graphics.drawable.LayerDrawable(
                new android.graphics.drawable.Drawable[]{track,progress});
        layers.setId(0,android.R.id.background);layers.setId(1,android.R.id.progress);
        for(int i=0;i<2;i++){layers.setLayerHeight(i,dp(4));layers.setLayerGravity(i,Gravity.CENTER_VERTICAL);}
        slider.setProgressTintList(null);slider.setProgressBackgroundTintList(null);slider.setProgressDrawable(layers);
        GradientDrawable thumb=new GradientDrawable();thumb.setShape(GradientDrawable.OVAL);
        thumb.setColor(TEXT);thumb.setStroke(dp(2),visibleAccent);thumb.setSize(dp(18),dp(18));
        slider.setThumbTintList(null);slider.setThumb(thumb);slider.setThumbOffset(dp(9));
        slider.setSplitTrack(false);slider.setPadding(dp(10),0,dp(10),0);
    }
    private void slider(LinearLayout parent,String label,int value,int min,int max,java.util.function.IntConsumer changed,String unit) {
        LinearLayout row=new LinearLayout(getContext());
        row.addView(text(label,13,TEXT,false),new LinearLayout.LayoutParams(0,-2,1));
        TextView amount=text(value+unit,12,accent,true);row.addView(amount);parent.addView(row);
        SeekBar seek=new SeekBar(getContext());seek.setMin(min);seek.setMax(max);seek.setProgress(value);
        seek.setContentDescription(label);styleSlider(seek);parent.addView(seek,new LinearLayout.LayoutParams(-1,dp(48)));
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            public void onProgressChanged(SeekBar bar,int progress,boolean user){amount.setText(progress+unit);}
            public void onStartTrackingTouch(SeekBar bar){}
            public void onStopTrackingTouch(SeekBar bar){feedback(bar);changed.accept(bar.getProgress());}
        });
    }
    private void toggle(LinearLayout parent,String title,String description,String key,boolean fallback) {
        LinearLayout row=new LinearLayout(getContext());row.setGravity(Gravity.CENTER_VERTICAL);row.setPadding(0,dp(8),0,dp(8));
        LinearLayout words=new LinearLayout(getContext());words.setOrientation(VERTICAL);words.addView(text(title,13,TEXT,false));
        if(!description.isEmpty()){TextView sub=text(description,11,MUTED,false);sub.setPadding(0,dp(4),dp(8),0);words.addView(sub);}
        words.setPadding(0,0,dp(8),0);
        row.addView(words,new LinearLayout.LayoutParams(0,-2,1));
        Switch control=new Switch(getContext());control.setContentDescription(title);control.setChecked(prefs.getBoolean(key,fallback));
        int[][] states={new int[]{android.R.attr.state_checked},new int[]{}};
        control.setTrackTintList(new ColorStateList(states,new int[]{controlAccent(),0xFF414650}));control.setThumbTintList(ColorStateList.valueOf(TEXT));
        control.setMinimumHeight(dp(48));control.setOnCheckedChangeListener((b,checked)->{prefs.edit().putBoolean(key,checked).apply();feedback(b);if(key.equals("show_days"))post(this::build);});
        row.addView(control);parent.addView(row);
    }
    private void feedback(View view){if(prefs.getBoolean("haptics",true))view.performHapticFeedback(android.view.HapticFeedbackConstants.CLOCK_TICK);}
    private TextView text(String value,int size,int color,boolean bold) {
        TextView t=new TextView(getContext());t.setText(value);t.setTextColor(color);
        t.setTextSize(size*prefs.getInt("text_scale",100)/100f);
        t.setIncludeFontPadding(false);t.setLineSpacing(dp(3),1f);
        t.setTypeface(Typeface.create(bold?"sans-serif-medium":"sans-serif",Typeface.NORMAL));return t;
    }
    private Button button(String value,boolean primary) {
        Button b=new Button(getContext());b.setText(value);b.setTextColor(new ColorStateList(new int[][]{new int[]{-android.R.attr.state_enabled},new int[]{}},new int[]{MUTED,TEXT}));b.setTextSize(13);b.setAllCaps(false);
        b.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));b.setMinWidth(0);b.setMinimumWidth(0);b.setPadding(dp(8),0,dp(8),0);
        b.setIncludeFontPadding(false);b.setLetterSpacing(.01f);b.setStateListAnimator(null);
        b.setBackground(interactiveSurface(primary?0xFF1D2028:CARD,10,primary?controlAccent():BORDER));return b;
    }
    private int controlAccent() {
        float[] hsv=new float[3];android.graphics.Color.colorToHSV(accent,hsv);
        hsv[1]=Math.min(hsv[1],.8f);hsv[2]=Math.max(hsv[2],.78f);
        return android.graphics.Color.HSVToColor(hsv);
    }
    private RippleDrawable interactiveSurface(int color,int radius,int border) {
        android.graphics.drawable.StateListDrawable states=new android.graphics.drawable.StateListDrawable();
        states.addState(new int[]{android.R.attr.state_focused},shape(color,radius,controlAccent()));
        states.addState(new int[]{},shape(color,radius,border));
        return new RippleDrawable(ColorStateList.valueOf(0x22FFFFFF),states,null);
    }
    private GradientDrawable shape(int color,int radius,int stroke){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));d.setStroke(dp(1),stroke);return d;}
    private void gap(LinearLayout parent,int height){parent.addView(new View(getContext()),new LinearLayout.LayoutParams(1,dp(height)));}
    private int dp(int value){return Math.round(value*getResources().getDisplayMetrics().density);}
}
