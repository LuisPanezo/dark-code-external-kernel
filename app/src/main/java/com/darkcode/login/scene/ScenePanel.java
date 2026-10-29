package com.darkcode.login.scene;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;
import android.widget.*;
import com.darkcode.login.ui.Palette;

public final class ScenePanel extends LinearLayout {
    public static final String[] SOURCES={"Propio · simulado","Unity · simulado","Unreal · simulado","Repetición de muestra"};
    public final SceneSurface scene;
    private final TextView status,details;
    private final LinearLayout[] pages=new LinearLayout[3];
    private final Button[] tabs=new Button[3];
    private int mode;
    private Spinner sources;
    public ScenePanel(Context context,boolean compact) {
        super(context);setOrientation(VERTICAL);setBackground(Palette.background(context,Palette.DARK,18));setClipToOutline(true);
        int p=Palette.dp(context,16);setPadding(p,p,p,p);
        TextView badge=Palette.text(context,"ESCENA DE PRUEBA",11,Palette.GOLD,true);
        badge.setLetterSpacing(.14f);addView(badge);
        TextView subtitle=Palette.text(context,"Datos simulados · ningún motor conectado",12,Palette.MUTED,false);
        subtitle.setPadding(0,Palette.dp(context,6),0,Palette.dp(context,12));addView(subtitle);
        scene=new SceneSurface(context);
        addView(scene,new LayoutParams(-1,Palette.dp(context,compact?170:230)));
        status=Palette.text(context,"Preparando escena…",12,Palette.GOLD,false);
        status.setPadding(0,Palette.dp(context,10),0,Palette.dp(context,10));addView(status);
        LinearLayout navigation=new LinearLayout(context);
        String[] labels={"Escena","Ajustes","Estado"};
        for(int i=0;i<3;++i) {
            final int tab=i;tabs[i]=Palette.button(context,labels[i],false);
            tabs[i].setOnClickListener(v->selectTab(tab));
            LayoutParams lp=new LayoutParams(0,Palette.dp(context,48),1);lp.setMargins(i==0?0:Palette.dp(context,6),0,0,0);
            navigation.addView(tabs[i],lp);
        }
        addView(navigation);
        for(int i=0;i<3;++i) {pages[i]=new LinearLayout(context);pages[i].setOrientation(VERTICAL);
            pages[i].setPadding(0,Palette.dp(context,14),0,0);addView(pages[i]);}
        pages[0].addView(Palette.text(context,"Fuente de la escena",13,Palette.CREAM,true));
        sources=new Spinner(context);
        ArrayAdapter<String> adapter=new ArrayAdapter<String>(context,android.R.layout.simple_spinner_item,SOURCES) {
            @Override public View getView(int position,View recycled,android.view.ViewGroup parent) {
                TextView view=(TextView)super.getView(position,recycled,parent);view.setTextColor(Palette.CREAM);view.setTextSize(14);return view;
            }
        };
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);sources.setAdapter(adapter);
        sources.setContentDescription("Fuente simulada");sources.setBackgroundTintList(ColorStateList.valueOf(Palette.GOLD));
        sources.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            public void onItemSelected(AdapterView<?> parent,View view,int position,long id) {mode=position;scene.setMode(position);}
            public void onNothingSelected(AdapterView<?> parent) {}
        });
        pages[0].addView(sources,new LayoutParams(-1,Palette.dp(context,48)));
        TextView legend=Palette.text(context,"● Ámbar     ● Jade     ● Índigo",12,Palette.MUTED,false);pages[0].addView(legend);
        addSwitch("Cuadrícula",true,scene::setGrid);
        addSwitch("Contornos",true,scene::setBounds);
        addSwitch("Animación",true,value->scene.setPaused(!value));
        details=Palette.text(context,"Esperando el primer frame",13,Palette.MUTED,false);pages[2].addView(details);
        scene.setListener((text,count)-> {
            status.setText(text+"  ·  "+count+" objetos");
            details.setText("Fuente: "+SOURCES[mode]+"\nEstado: "+text+"\nObjetos: "+count+
                "\n\nLa escena de prueba no necesita iniciar sesión.");
        });
        selectTab(0);
    }
    private interface Toggle {void set(boolean value);}
    private void addSwitch(String label,boolean checked,Toggle action) {
        Switch s=new Switch(getContext());s.setText(label);s.setTextColor(Palette.CREAM);s.setTextSize(14);
        s.setChecked(checked);s.setThumbTintList(ColorStateList.valueOf(Palette.GOLD));
        s.setOnCheckedChangeListener((button,value)->action.set(value));
        pages[1].addView(s,new LayoutParams(-1,Palette.dp(getContext(),48)));
    }
    public void selectTab(int tab) {
        for(int i=0;i<3;++i) {pages[i].setVisibility(i==tab?VISIBLE:GONE);tabs[i].setTextColor(i==tab?Palette.GOLD:Palette.MUTED);tabs[i].setSelected(i==tab);}
    }
    public void selectSource(int source) {mode=source;scene.setMode(source);sources.setSelection(source);}
    public void resume() {scene.onResume();}
    public void pause() {scene.onPause();}
    public void release() {scene.release();}
}
