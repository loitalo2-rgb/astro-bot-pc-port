import os
import sys
import subprocess
import configparser
import tkinter as tk
from tkinter import ttk, messagebox

WORKSPACE_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(WORKSPACE_DIR, "astro_config.ini")
HYBRID_DIR = r"D:\Jogos ps4\AstroHybrid\AstroBotPC-AstroHybrid-1.0.9"
RUN_HYBRID_DIR = os.path.join(HYBRID_DIR, "Build", "RunHybrid")
PDF_REPORT = os.path.join(WORKSPACE_DIR, "Relatorio_Completo_AstroBot_PC.pdf")
LAUNCH_BAT = os.path.join(HYBRID_DIR, "Launch.bat")
WORKSPACE_BAT = os.path.join(WORKSPACE_DIR, "Iniciar_AstroBot_PC.bat")

def load_config():
    config = configparser.ConfigParser()
    if os.path.exists(CONFIG_PATH):
        config.read(CONFIG_PATH, encoding="utf-8")
    else:
        config["Graphics"] = {"width": "2560", "height": "1440", "fullscreen": "1", "vsync": "1", "fps_limit": "60", "perf_display": "0"}
        config["Upscaler"] = {"backend": "fsr41", "quality": "performance", "sharpness": "85", "frame_generation": "1"}
        config["Controls"] = {"pad_type": "dualsense", "trigger_state": "1", "pad_motion": "1", "rumble_scale": "100"}
        config["Engine_GPU"] = {
            "shader_cache": "1", "cache_title_id": "PPSA21567", "shader_precompile": "1",
            "shader_precompile_threads": "8", "pipeline_prefetch": "1", "pipeline_prewarm": "1",
            "stage_prep_parallel": "1", "texture_async_staging": "1", "submission_coalesce": "1",
            "barrier_batch": "1", "rt_mode": "0", "rt_stub": "1", "buffer_range_fixes": "1",
            "commit_epoch_fastpath": "0"
        }
        config["CPU"] = {"cp_priority": "2", "draw_prep_priority": "2"}
        save_config(config)
    return config

def save_config(config):
    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        config.write(f)

class AstroLauncherApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Astro Bot PC - Launcher & Otimizador de FPS (By: @loitalo2-rgb)")
        self.geometry("700x670")
        self.resizable(False, False)
        self.config = load_config()
        self.configure(bg="#121826")
        
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TLabel", background="#121826", foreground="#E2E8F0", font=("Segoe UI", 10))
        style.configure("Header.TLabel", background="#121826", foreground="#38BDF8", font=("Segoe UI", 15, "bold"))
        style.configure("SubHeader.TLabel", background="#121826", foreground="#94A3B8", font=("Segoe UI", 9))
        style.configure("TCheckbutton", background="#121826", foreground="#E2E8F0", font=("Segoe UI", 10))
        style.configure("TCombobox", fieldbackground="#1E293B", background="#334155", foreground="#F8FAFC", font=("Segoe UI", 10))
        
        self.create_widgets()

    def create_widgets(self):
        header = ttk.Label(self, text="ASTRO BOT PC (Port Nativo PS5)", style="Header.TLabel")
        header.pack(pady=(10, 1))
        sub = ttk.Label(self, text="Otimizador de FPS nos Mundos, Pré-carregamento de Shaders & FSR/XeSS", style="SubHeader.TLabel")
        sub.pack(pady=(0, 8))

        main_frame = tk.Frame(self, bg="#1E293B", padx=16, pady=10, relief="ridge", bd=1)
        main_frame.pack(fill="both", expand=True, padx=16, pady=(0, 8))

        # 1. Resolução
        lbl_res = ttk.Label(main_frame, text="Resolução Alvo:")
        lbl_res.grid(row=0, column=0, sticky="w", pady=4)
        self.res_var = tk.StringVar()
        w = self.config.get("Graphics", "width", fallback="2560")
        self.res_cb = ttk.Combobox(main_frame, textvariable=self.res_var, state="readonly", width=30)
        self.res_cb["values"] = ("1920x1080 (Full HD - Mais Fluido)", "2560x1440 (2K Quad HD - Recomendado)", "3840x2160 (4K Ultra HD)")
        if "3840" in w:
            self.res_cb.current(2)
        elif "2560" in w:
            self.res_cb.current(1)
        else:
            self.res_cb.current(0)
        self.res_cb.grid(row=0, column=1, sticky="e", pady=4)

        # 2. Upscaler Backend
        lbl_up = ttk.Label(main_frame, text="Upscaler Dinâmico:")
        lbl_up.grid(row=1, column=0, sticky="w", pady=4)
        self.up_var = tk.StringVar()
        self.up_cb = ttk.Combobox(main_frame, textvariable=self.up_var, state="readonly", width=30)
        self.up_cb["values"] = (
            "AMD FSR 4.1 (Modelo Neural IA + RCAS)",
            "AMD FSR 3.1 (Temporal + Frame Gen)",
            "Intel XeSS (Super Sampling 1.3+)",
            "Nativo (Sem Upscaler)"
        )
        up_cfg = self.config.get("Upscaler", "backend", fallback="fsr41")
        if up_cfg == "fsr31":
            self.up_cb.current(1)
        elif up_cfg == "xess":
            self.up_cb.current(2)
        elif up_cfg == "native":
            self.up_cb.current(3)
        else:
            self.up_cb.current(0)
        self.up_cb.grid(row=1, column=1, sticky="e", pady=4)

        # 3. Qualidade do Upscaler
        lbl_qual = ttk.Label(main_frame, text="Qualidade do Upscaler:")
        lbl_qual.grid(row=2, column=0, sticky="w", pady=4)
        self.qual_var = tk.StringVar()
        self.qual_cb = ttk.Combobox(main_frame, textvariable=self.qual_var, state="readonly", width=30)
        self.qual_cb["values"] = ("Desempenho (2.0x - Máximo FPS)", "Equilibrado (1.7x)", "Qualidade (1.5x - Melhor Visual)", "Native AA (1.0x)")
        q_cfg = self.config.get("Upscaler", "quality", fallback="performance")
        if q_cfg == "balanced":
            self.qual_cb.current(1)
        elif q_cfg == "quality":
            self.qual_cb.current(2)
        elif q_cfg == "native_aa":
            self.qual_cb.current(3)
        else:
            self.qual_cb.current(0)
        self.qual_cb.grid(row=2, column=1, sticky="e", pady=4)

        # 4. Taxa de Quadros / VSync
        lbl_fps = ttk.Label(main_frame, text="Taxa de Quadros:")
        lbl_fps.grid(row=3, column=0, sticky="w", pady=4)
        self.fps_var = tk.StringVar()
        self.fps_cb = ttk.Combobox(main_frame, textvariable=self.fps_var, state="readonly", width=30)
        self.fps_cb["values"] = ("60 FPS (VSync Estável)", "120 FPS (Alta Fluidez)", "Desbloqueado (FreeSync/G-Sync)")
        fps_cfg = self.config.get("Graphics", "fps_limit", fallback="60")
        if fps_cfg == "120":
            self.fps_cb.current(1)
        elif fps_cfg == "0":
            self.fps_cb.current(2)
        else:
            self.fps_cb.current(0)
        self.fps_cb.grid(row=3, column=1, sticky="e", pady=4)

        # 5. Tipo de Controle
        lbl_pad = ttk.Label(main_frame, text="Perfil de Controle:")
        lbl_pad.grid(row=4, column=0, sticky="w", pady=4)
        self.pad_var = tk.StringVar()
        self.pad_cb = ttk.Combobox(main_frame, textvariable=self.pad_var, state="readonly", width=30)
        self.pad_cb["values"] = ("DualSense PS5 (Hápticos + Gatilhos L2/R2)", "Controle Xbox / XInput", "Teclado e Mouse")
        pad_cfg = self.config.get("Controls", "pad_type", fallback="xbox")
        if pad_cfg == "dualsense":
            self.pad_cb.current(0)
        elif pad_cfg == "keyboard":
            self.pad_cb.current(2)
        else:
            self.pad_cb.current(1)
        self.pad_cb.grid(row=4, column=1, sticky="e", pady=4)

        # 6. OTIMIZAÇÃO CRÍTICA DE FPS: Desativar Software Ray Tracing
        self.rt_fast_var = tk.BooleanVar(value=(self.config.get("Engine_GPU", "rt_mode", fallback="0") == "0"))
        chk_rt = tk.Checkbutton(main_frame, text="🚀 Modo FPS Alto: Ignorar Ray-Tracing por Software (Resolve FPS baixo nos mundos)",
                                variable=self.rt_fast_var, bg="#1E293B", fg="#38BDF8", font=("Segoe UI", 9, "bold"),
                                selectcolor="#0F172A", activebackground="#1E293B", activeforeground="#38BDF8")
        chk_rt.grid(row=5, column=0, columnspan=2, sticky="w", pady=4)

        # 7. PRÉ-CARREGAR SHADERS (PAPP / PROGRAM CACHE)
        self.precompile_var = tk.BooleanVar(value=(self.config.get("Engine_GPU", "shader_precompile", fallback="1") == "1"))
        chk_pre = tk.Checkbutton(main_frame, text="⚡ Pré-carregar e Compilar Shaders em 8 Threads (Elimina micro-travadas)",
                                 variable=self.precompile_var, bg="#1E293B", fg="#E2E8F0", selectcolor="#0F172A",
                                 activebackground="#1E293B", activeforeground="#38BDF8")
        chk_pre.grid(row=6, column=0, columnspan=2, sticky="w", pady=3)

        # 8. Checkbox Frame Generation
        self.fg_var = tk.BooleanVar(value=(self.config.get("Upscaler", "frame_generation", fallback="1") == "1"))
        chk_fg = tk.Checkbutton(main_frame, text="Geração de Quadros por IA (Frame Generation FSR 3.1/4.1)",
                                variable=self.fg_var, bg="#1E293B", fg="#E2E8F0", selectcolor="#0F172A",
                                activebackground="#1E293B", activeforeground="#38BDF8")
        chk_fg.grid(row=7, column=0, columnspan=2, sticky="w", pady=3)

        # 9. Checkbox Giroscópio
        self.motion_var = tk.BooleanVar(value=(self.config.get("Controls", "pad_motion", fallback="1") == "1"))
        chk_motion = tk.Checkbutton(main_frame, text="Ativar Giroscópio / Sensor de Movimento (Shake & Tilt)",
                                    variable=self.motion_var, bg="#1E293B", fg="#E2E8F0", selectcolor="#0F172A",
                                    activebackground="#1E293B", activeforeground="#38BDF8")
        chk_motion.grid(row=8, column=0, columnspan=2, sticky="w", pady=3)

        # 10. Checkbox HUD de Desempenho
        self.perf_var = tk.BooleanVar(value=(self.config.get("Graphics", "perf_display", fallback="0") == "1"))
        chk_perf = tk.Checkbutton(main_frame, text="Exibir HUD de Desempenho Nativo (FPS, CPU/GPU, VRAM)",
                                  variable=self.perf_var, bg="#1E293B", fg="#E2E8F0", selectcolor="#0F172A",
                                  activebackground="#1E293B", activeforeground="#38BDF8")
        chk_perf.grid(row=9, column=0, columnspan=2, sticky="w", pady=3)

        # Botões inferiores
        btn_frame = tk.Frame(self, bg="#121826")
        btn_frame.pack(fill="x", padx=16, pady=(0, 10))

        btn_launch = tk.Button(btn_frame, text="🚀 INICIAR ASTRO BOT (ALTA PERFORMANCE)", font=("Segoe UI", 11, "bold"),
                               bg="#0284C7", fg="#FFFFFF", activebackground="#0369A1", activeforeground="#FFFFFF",
                               padx=15, pady=8, bd=0, cursor="hand2", command=self.launch_game)
        btn_launch.pack(fill="x", pady=(0, 6))

        sub_btn_frame = tk.Frame(btn_frame, bg="#121826")
        sub_btn_frame.pack(fill="x")

        btn_save = tk.Button(sub_btn_frame, text="💾 Salvar Configurações", font=("Segoe UI", 9),
                             bg="#334155", fg="#F8FAFC", bd=0, padx=10, pady=5, cursor="hand2", command=lambda: self.apply_settings(show_message=True))
        btn_save.pack(side="left", expand=True, fill="x", padx=(0, 4))

        btn_pdf = tk.Button(sub_btn_frame, text="📄 Abrir Relatório PDF", font=("Segoe UI", 9),
                            bg="#334155", fg="#F8FAFC", bd=0, padx=10, pady=5, cursor="hand2", command=self.open_pdf)
        btn_pdf.pack(side="left", expand=True, fill="x", padx=(4, 0))

    def apply_settings(self, show_message=False):
        sel_res = self.res_var.get()
        if "3840" in sel_res:
            w, h = "3840", "2160"
        elif "2560" in sel_res:
            w, h = "2560", "1440"
        else:
            w, h = "1920", "1080"
        
        self.config["Graphics"]["width"] = w
        self.config["Graphics"]["height"] = h

        sel_up = self.up_var.get()
        if "FSR 4.1" in sel_up:
            self.config["Upscaler"]["backend"] = "fsr41"
        elif "FSR 3.1" in sel_up:
            self.config["Upscaler"]["backend"] = "fsr31"
        elif "XeSS" in sel_up:
            self.config["Upscaler"]["backend"] = "xess"
        else:
            self.config["Upscaler"]["backend"] = "native"

        sel_q = self.qual_var.get()
        if "Equilibrado" in sel_q:
            self.config["Upscaler"]["quality"] = "balanced"
        elif "Qualidade" in sel_q:
            self.config["Upscaler"]["quality"] = "quality"
        elif "Native AA" in sel_q:
            self.config["Upscaler"]["quality"] = "native_aa"
        else:
            self.config["Upscaler"]["quality"] = "performance"

        self.config["Upscaler"]["frame_generation"] = "1" if self.fg_var.get() else "0"

        sel_fps = self.fps_var.get()
        if "120" in sel_fps:
            self.config["Graphics"]["fps_limit"] = "120"
        elif "Desbloqueado" in sel_fps:
            self.config["Graphics"]["fps_limit"] = "0"
            self.config["Graphics"]["vsync"] = "0"
        else:
            self.config["Graphics"]["fps_limit"] = "60"
            self.config["Graphics"]["vsync"] = "1"

        sel_pad = self.pad_var.get()
        if "DualSense" in sel_pad:
            self.config["Controls"]["pad_type"] = "dualsense"
            self.config["Controls"]["trigger_state"] = "1"
        elif "Teclado" in sel_pad:
            self.config["Controls"]["pad_type"] = "keyboard"
        else:
            self.config["Controls"]["pad_type"] = "xbox"
            self.config["Controls"]["trigger_state"] = "0"

        self.config["Controls"]["pad_motion"] = "1" if self.motion_var.get() else "0"
        self.config["Engine_GPU"]["shader_cache"] = "1"
        self.config["Engine_GPU"]["shader_precompile"] = "1" if self.precompile_var.get() else "0"
        self.config["Engine_GPU"]["rt_mode"] = "0" if self.rt_fast_var.get() else "1"
        self.config["Engine_GPU"]["rt_stub"] = "1" if self.rt_fast_var.get() else "0"
        self.config["Graphics"]["perf_display"] = "1" if self.perf_var.get() else "0"

        save_config(self.config)
        self.write_launcher_bats()
        if show_message:
            messagebox.showinfo("Configurações Salvas", "Configurações de FPS, Shaders e Vídeo salvas com sucesso!")

    def write_launcher_bats(self):
        w = self.config.get("Graphics", "width", fallback="2560")
        h = self.config.get("Graphics", "height", fallback="1440")
        up = self.config.get("Upscaler", "backend", fallback="fsr41")
        qual = self.config.get("Upscaler", "quality", fallback="performance")
        fg = self.config.get("Upscaler", "frame_generation", fallback="1")
        motion = self.config.get("Controls", "pad_motion", fallback="1")
        trig = self.config.get("Controls", "trigger_state", fallback="0")
        
        rt_fast = (self.config.get("Engine_GPU", "rt_mode", fallback="0") == "0")
        rt_soft = "0" if rt_fast else "auto"
        rt_stub = "1" if rt_fast else "0"
        
        precompile = self.config.get("Engine_GPU", "shader_precompile", fallback="1")
        perf = self.config.get("Graphics", "perf_display", fallback="0")
        perf_flag = "true" if perf == "1" else "false"

        bat_content = f"""@echo off
title Astro Bot PC - Running
cd /d "{RUN_HYBRID_DIR}"

set APS5_GPU_ENGINE=kyty
set "KYTY_BARRIER_BATCH="
set "KYTY_DISPATCH_OVERLAP="
set "KYTY_WAVE64_WARPS="
set "ASTROHYBRID_COMMIT_EPOCH_FASTPATH=0"
set "KYTY_BUFFER_RANGE_FIXES=1"
set KYTY_HANG_WATCHDOG=0
set ASTROHYBRID_CACHE_TITLE_ID=PPSA21567
set KYTY_GPU_HOST_MODULE=astrohybrid_host.dll
set APS5_PAD_TRIGGER_STATE={trig}
set APS5_SAVEDATA_TRANSFER_FALLBACK=1
set ASTROHYBRID_PAD_MOTION={motion}
set ASTROHYBRID_SHADER_CACHE=1

rem OTIMIZACOES DE FPS NOS MUNDOS E PRE-COMPILACAO DE SHADERS
set KYTY_RT_SOFTWARE=0
set KYTY_RT_STUB=1
set KYTY_CPU_RESERVE=off
set ASTROHYBRID_GAME_SPEED=1
set ASTROHYBRID_PRIORITY=abovenormal
set KYTY_SHADER_PRECOMPILE={precompile}
set KYTY_SHADER_PRECOMPILE_THREADS=8
set KYTY_PIPELINE_PREWARM_COMPUTE=1
set KYTY_PIPELINE_PREFETCH=1
set KYTY_PIPELINE_PREFETCH_PROGRAMS=1
set KYTY_STAGE_PREP_PARALLEL=1
set KYTY_TEXTURE_ASYNC_STAGING=1
set KYTY_SUBMISSION_COALESCE=1
set KYTY_PENDING_REFRESH_US=50
set KYTY_EOP_FLUSH_BATCH=16

set ASTRO_UPSCALER_BACKEND={up}
set ASTRO_UPSCALER_QUALITY={qual}
set ASTRO_FRAME_GENERATION={fg}

set KYTY_CP_PRIORITY=2
set KYTY_DRAW_PREP_PRIORITY=2
set KYTY_DRAW_PREP_SPIN_US=10
set KYTY_DRAW_PREP_COLD_SPIN_US=5
set KYTY_GPU_WIDTH={w}
set KYTY_GPU_HEIGHT={h}

if exist "u59-preset.cmd" call "u59-preset.cmd"

echo ========================================================
echo   Iniciando Astro Bot PC ({w}x{h}) - Modo Alta Performance
echo   Ray-Tracing: {"DESATIVADO (FPS MAXIMO)" if rt_fast else "SOFTWARE RT"}
echo   Shader Precompile: {"8 THREADS ATIVAS" if precompile == "1" else "DESATIVADO"}
echo   Upscaler: {up.upper()} ({qual.upper()}) ^| FrameGen: {fg}
echo   GPU: AMD Radeon RX 9060 XT (16 GB VRAM)
echo ========================================================
AstroBot.exe -perfDisplay {perf_flag} %*
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo O jogo encerrou com codigo %ERRORLEVEL%
    pause
)
"""
        with open(LAUNCH_BAT, "w", encoding="utf-8") as f:
            f.write(bat_content)
        with open(WORKSPACE_BAT, "w", encoding="utf-8") as f:
            f.write(bat_content)

    def launch_game(self):
        try:
            self.apply_settings(show_message=False)
            os.startfile(LAUNCH_BAT)
            self.destroy()
        except Exception as e:
            try:
                subprocess.Popen(["cmd.exe", "/c", "start", "Astro Bot PC", LAUNCH_BAT], shell=True)
                self.destroy()
            except Exception as e2:
                messagebox.showerror("Erro ao Iniciar", f"Não foi possível abrir o jogo:\n{e}\n{e2}")

    def open_pdf(self):
        if os.path.exists(PDF_REPORT):
            os.startfile(PDF_REPORT)
        else:
            messagebox.showwarning("Arquivo não encontrado", "O relatório PDF não foi encontrado no local esperado.")

if __name__ == "__main__":
    app = AstroLauncherApp()
    app.mainloop()
