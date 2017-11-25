;;;

;;; Code:
(eval-when-compile (require 'cl-lib))
(or (featurep 'gtk3wl)
    (error "%s: Loading gtk3wl-win.el but not compiled for Gtk+-3 with wayland."
           (invocation-name)))

;; Documentation-purposes only: actually loaded in loadup.el.
(require 'frame)
(require 'mouse)
(require 'faces)
(require 'menu-bar)
(require 'fontset)
(require 'dnd)

(defgroup gtk3wl nil
  "GNUstep/macOS specific features."
  :group 'environment)

;;;; Command line argument handling.

(defvar x-invocation-args)
;; Set in term/common-win.el; currently unused by Gtk's x-open-connection.
(defvar x-command-line-resources)

;; gtk3wlterm.c.
(defvar gtk3wl-input-file)

(defun gtk3wl-handle-nxopen (_switch &optional temp)
  (setq unread-command-events (append unread-command-events
                                      (if temp '(gtk3wl-open-temp-file)
                                        '(gtk3wl-open-file)))
        gtk3wl-input-file (append gtk3wl-input-file (list (pop x-invocation-args)))))

(defun gtk3wl-handle-nxopentemp (switch)
  (gtk3wl-handle-nxopen switch t))

(defun gtk3wl-ignore-1-arg (_switch)
  (setq x-invocation-args (cdr x-invocation-args)))

(defun gtk3wl-parse-geometry (geom)
  "Parse a Nextstep-style geometry string GEOM.
Returns an alist of the form ((top . TOP), (left . LEFT) ... ).
The properties returned may include `top', `left', `height', and `width'."
  (when (string-match "\\([0-9]+\\)\\( \\([0-9]+\\)\\( \\([0-9]+\\)\
\\( \\([0-9]+\\) ?\\)?\\)?\\)?"
		      geom)
    (apply
     'append
     (list
      (list (cons 'top (string-to-number (match-string 1 geom))))
      (if (match-string 3 geom)
	  (list (cons 'left (string-to-number (match-string 3 geom)))))
      (if (match-string 5 geom)
	  (list (cons 'height (string-to-number (match-string 5 geom)))))
      (if (match-string 7 geom)
	  (list (cons 'width (string-to-number (match-string 7 geom)))))))))

;;;; Keyboard mapping.

(define-obsolete-variable-alias 'gtk3wl-alternatives-map 'x-alternatives-map "24.1")

;; Here are some Nextstep-like bindings for command key sequences.
(define-key global-map [?\s-,] 'customize)
(define-key global-map [?\s-'] 'next-multiframe-window)
(define-key global-map [?\s-`] 'other-frame)
(define-key global-map [?\s-~] 'gtk3wl-prev-frame)
(define-key global-map [?\s--] 'center-line)
(define-key global-map [?\s-:] 'ispell)
(define-key global-map [?\s-?] 'info)
(define-key global-map [?\s-^] 'kill-some-buffers)
(define-key global-map [?\s-&] 'kill-current-buffer)
(define-key global-map [?\s-C] 'gtk3wl-popup-color-panel)
(define-key global-map [?\s-D] 'dired)
(define-key global-map [?\s-E] 'edit-abbrevs)
(define-key global-map [?\s-L] 'shell-command)
(define-key global-map [?\s-M] 'manual-entry)
(define-key global-map [?\s-S] 'gtk3wl-write-file-using-panel)
(define-key global-map [?\s-a] 'mark-whole-buffer)
(define-key global-map [?\s-c] 'gtk3wl-copy-including-secondary)
(define-key global-map [?\s-d] 'isearch-repeat-backward)
(define-key global-map [?\s-e] 'isearch-yank-kill)
(define-key global-map [?\s-f] 'isearch-forward)
(define-key global-map [?\s-g] 'isearch-repeat-forward)
(define-key global-map [?\s-h] 'gtk3wl-do-hide-emacs)
(define-key global-map [?\s-H] 'gtk3wl-do-hide-others)
(define-key global-map [?\M-\s-h] 'gtk3wl-do-hide-others)
(define-key key-translation-map [?\M-\s-\u02D9] [?\M-\s-h])
(define-key global-map [?\s-j] 'exchange-point-and-mark)
(define-key global-map [?\s-k] 'kill-current-buffer)
(define-key global-map [?\s-l] 'goto-line)
(define-key global-map [?\s-m] 'iconify-frame)
(define-key global-map [?\s-n] 'make-frame)
(define-key global-map [?\s-o] 'gtk3wl-open-file-using-panel)
(define-key global-map [?\s-p] 'gtk3wl-print-buffer)
(define-key global-map [?\s-q] 'save-buffers-kill-emacs)
(define-key global-map [?\s-s] 'save-buffer)
(define-key global-map [?\s-t] 'gtk3wl-popup-font-panel)
(define-key global-map [?\s-u] 'revert-buffer)
(define-key global-map [?\s-v] 'yank)
(define-key global-map [?\s-w] 'delete-frame)
(define-key global-map [?\s-x] 'kill-region)
(define-key global-map [?\s-y] 'gtk3wl-paste-secondary)
(define-key global-map [?\s-z] 'undo)
(define-key global-map [?\s-|] 'shell-command-on-region)
(define-key global-map [s-kp-bar] 'shell-command-on-region)
;; (as in Terminal.app)
(define-key global-map [s-right] 'gtk3wl-next-frame)
(define-key global-map [s-left] 'gtk3wl-prev-frame)

(define-key global-map [home] 'beginning-of-buffer)
(define-key global-map [end] 'end-of-buffer)
(define-key global-map [kp-home] 'beginning-of-buffer)
(define-key global-map [kp-end] 'end-of-buffer)
(define-key global-map [kp-prior] 'scroll-down-command)
(define-key global-map [kp-next] 'scroll-up-command)

;; Allow shift-clicks to work similarly to under Nextstep.
(define-key global-map [S-mouse-1] 'mouse-save-then-kill)
(global-unset-key [S-down-mouse-1])

;; Special Nextstep-generated events are converted to function keys.  Here
;; are the bindings for them.  Note, these keys are actually declared in
;; x-setup-function-keys in common-win.
(define-key global-map [gtk3wl-power-off] 'save-buffers-kill-emacs)
(define-key global-map [gtk3wl-open-file] 'gtk3wl-find-file)
(define-key global-map [gtk3wl-open-temp-file] [gtk3wl-open-file])
(define-key global-map [gtk3wl-change-font] 'gtk3wl-respond-to-change-font)
(define-key global-map [gtk3wl-open-file-line] 'gtk3wl-open-file-select-line)
(define-key global-map [gtk3wl-spi-service-call] 'gtk3wl-spi-service-call)
(define-key global-map [gtk3wl-new-frame] 'make-frame)
(define-key global-map [gtk3wl-toggle-toolbar] 'gtk3wl-toggle-toolbar)
(define-key global-map [gtk3wl-show-prefs] 'customize)


;; Set up a number of aliases and other layers to pretend we're using
;; the Choi/Mitsuharu Carbon port.

(defvaralias 'mac-allow-anti-aliasing 'gtk3wl-antialias-text)
(defvaralias 'mac-command-modifier 'gtk3wl-command-modifier)
(defvaralias 'mac-right-command-modifier 'gtk3wl-right-command-modifier)
(defvaralias 'mac-control-modifier 'gtk3wl-control-modifier)
(defvaralias 'mac-right-control-modifier 'gtk3wl-right-control-modifier)
(defvaralias 'mac-option-modifier 'gtk3wl-option-modifier)
(defvaralias 'mac-right-option-modifier 'gtk3wl-right-option-modifier)
(defvaralias 'mac-function-modifier 'gtk3wl-function-modifier)
(declare-function gtk3wl-do-applescript "gtk3wlfns.c" (script))
(defalias 'do-applescript 'gtk3wl-do-applescript)

;;;; Services
(declare-function gtk3wl-perform-service "gtk3wlfns.c" (service send))

(defun gtk3wl-define-service (path)
  (let ((mapping [menu-bar services])
	(service (mapconcat 'identity path "/"))
	(name (intern
               (subst-char-in-string
                ?\s ?-
                (mapconcat 'identity (cons "gtk3wl-service" path) "-")))))
    ;; This defines the function.
    (defalias name
      (lambda (arg)
        (interactive "p")
        (let* ((in-string
                (cond ((stringp arg) arg)
                      (mark-active
                       (buffer-substring (region-beginning) (region-end)))))
               (out-string (gtk3wl-perform-service service in-string)))
          (cond
           ((stringp arg) out-string)
           ((and out-string (or (not in-string)
                                (not (string= in-string out-string))))
            (if mark-active (delete-region (region-beginning) (region-end)))
            (insert out-string)
            (setq deactivate-mark nil))))))
    (cond
     ((lookup-key global-map mapping)
      (while (cdr path)
	(setq mapping (vconcat mapping (list (intern (car path)))))
	(if (not (keymapp (lookup-key global-map mapping)))
	    (define-key global-map mapping
	      (cons (car path) (make-sparse-keymap (car path)))))
	(setq path (cdr path)))
      (setq mapping (vconcat mapping (list (intern (car path)))))
      (define-key global-map mapping (cons (car path) name))))
    name))

;; gtk3wlterm.c
(defvar gtk3wl-input-spi-name)
(defvar gtk3wl-input-spi-arg)

(declare-function dnd-open-file "dnd" (uri action))

;; Handles multiline strings that are passed to the "open-file" service.
(defun gtk3wl-open-file-service (filenames)
  "Open multiple files when selecting a multiline string FILENAMES."
  (let ((filelist (split-string filenames "[\n\r]+" t "[ \u00A0\t]+")))
    ;; The path strings are trimmed for spaces, nbsp and tabs.
    (dolist (filestring filelist)
      (dnd-open-file filestring nil))))


(defun gtk3wl-spi-service-call ()
  "Respond to a service request."
  (interactive)
  (cond ((string-equal gtk3wl-input-spi-name "open-selection")
	 (switch-to-buffer (generate-new-buffer "*untitled*"))
	 (insert gtk3wl-input-spi-arg))
	((string-equal gtk3wl-input-spi-name "open-file")
	 (gtk3wl-open-file-service gtk3wl-input-spi-arg))
	((string-equal gtk3wl-input-spi-name "mail-selection")
	 (compose-mail)
	 (rfc822-goto-eoh)
	 (forward-line 1)
	 (insert gtk3wl-input-spi-arg))
	((string-equal gtk3wl-input-spi-name "mail-to")
	 (compose-mail gtk3wl-input-spi-arg))
	(t (error "Service %s not recognized" gtk3wl-input-spi-name))))


;; Composed key sequence handling for Nextstep system input methods.
;; (On Nextstep systems, input methods are provided for CJK
;; characters, etc. which require multiple keystrokes, and during
;; entry a partial ("working") result is typically shown in the
;; editing window.)

(defface gtk3wl-working-text-face
  '((t :underline t))
  "Face used to highlight working text during compose sequence insert."
  :group 'gtk3wl)

(defvar gtk3wl-working-overlay nil
  "Overlay used to highlight working text during compose sequence insert.
When text is in th echo area, this just stores the length of the working text.")

(defvar gtk3wl-working-text)		; gtk3wlterm.c

;; Test if in echo area, based on mac-win.el 2007/08/26 unicode-2.
;; This will fail if called from a NONASCII_KEYSTROKE event on the global map.
(defun gtk3wl-in-echo-area ()
  "Whether, for purposes of inserting working composition text, the minibuffer
is currently being used."
  (or isearch-mode
      (and cursor-in-echo-area (current-message))
      ;; Overlay strings are not shown in some cases.
      (get-char-property (point) 'invisible)
      (and (not (bobp))
	   (or (and (get-char-property (point) 'display)
		    (eq (get-char-property (1- (point)) 'display)
			(get-char-property (point) 'display)))
	       (and (get-char-property (point) 'composition)
		    (eq (get-char-property (1- (point)) 'composition)
			(get-char-property (point) 'composition)))))))

;; The 'interactive' here stays for subinvocations, so the gtk3wl-in-echo-area
;; always returns nil for some reason.  If this WASN'T the case, we could
;; map this to [gtk3wl-insert-working-text] and eliminate Fevals in gtk3wlterm.c.
;; These functions test whether in echo area and delegate accordingly.
(defun gtk3wl-put-working-text ()
  (interactive)
  (if (gtk3wl-in-echo-area) (gtk3wl-echo-working-text) (gtk3wl-insert-working-text)))
(defun gtk3wl-unput-working-text ()
  (interactive)
  (gtk3wl-delete-working-text))

(defun gtk3wl-insert-working-text ()
  "Insert contents of `gtk3wl-working-text' as UTF-8 string and mark with
`gtk3wl-working-overlay'.  Any previously existing working text is cleared first.
The overlay is assigned the face `gtk3wl-working-text-face'."
  ;; FIXME: if buffer is read-only, don't try to insert anything
  ;;  and if text is bound to a command, execute that instead (Bug#1453)
  (interactive)
  (gtk3wl-delete-working-text)
  (let ((start (point)))
    (insert gtk3wl-working-text)
    (overlay-put (setq gtk3wl-working-overlay (make-overlay start (point)
							(current-buffer) nil t))
		 'face 'gtk3wl-working-text-face)))

(defun gtk3wl-echo-working-text ()
  "Echo contents of `gtk3wl-working-text' in message display area.
See `gtk3wl-insert-working-text'."
  (gtk3wl-delete-working-text)
  (let* ((msg (current-message))
	 (msglen (length msg))
	 message-log-max)
    (setq gtk3wl-working-overlay (length gtk3wl-working-text))
    (setq msg (concat msg gtk3wl-working-text))
    (put-text-property msglen (+ msglen gtk3wl-working-overlay)
		       'face 'gtk3wl-working-text-face msg)
    (message "%s" msg)))

(defun gtk3wl-delete-working-text()
  "Delete working text and clear `gtk3wl-working-overlay'."
  (interactive)
  (cond
   ((and (overlayp gtk3wl-working-overlay)
         ;; Still alive?
         (overlay-buffer gtk3wl-working-overlay))
    (with-current-buffer (overlay-buffer gtk3wl-working-overlay)
      (delete-region (overlay-start gtk3wl-working-overlay)
                     (overlay-end gtk3wl-working-overlay))
      (delete-overlay gtk3wl-working-overlay)))
   ((integerp gtk3wl-working-overlay)
    (let ((msg (current-message))
          message-log-max)
      (setq msg (substring msg 0 (- (length msg) gtk3wl-working-overlay)))
      (message "%s" msg))))
  (setq gtk3wl-working-overlay nil))


;; macOS file system Unicode UTF-8 NFD (decomposed form) support.
(when (eq system-type 'darwin)
  ;; Used prior to Emacs 25.
  (define-coding-system-alias 'utf-8-nfd 'utf-8-hfs)

  (set-file-name-coding-system 'utf-8-hfs))

;;;; Inter-app communications support.

(defun gtk3wl-insert-file ()
  "Insert contents of file `gtk3wl-input-file' like insert-file but with less
prompting.  If file is a directory perform a `find-file' on it."
  (interactive)
  (let ((f (pop gtk3wl-input-file)))
    (if (file-directory-p f)
        (find-file f)
      (push-mark (+ (point) (cadr (insert-file-contents f)))))))

(defvar gtk3wl-select-overlay nil
  "Overlay used to highlight areas in files requested by Nextstep apps.")
(make-variable-buffer-local 'gtk3wl-select-overlay)

(defvar gtk3wl-input-line) 			; gtk3wlterm.c

(defun gtk3wl-open-file-select-line ()
  "Open a buffer containing the file `gtk3wl-input-file'.
Lines are highlighted according to `gtk3wl-input-line'."
  (interactive)
  (gtk3wl-find-file)
  (cond
   ((and gtk3wl-input-line (buffer-modified-p))
    (if gtk3wl-select-overlay
        (setq gtk3wl-select-overlay (delete-overlay gtk3wl-select-overlay)))
    (deactivate-mark)
    (goto-char (point-min))
    (forward-line (1- (if (consp gtk3wl-input-line)
                          (min (car gtk3wl-input-line) (cdr gtk3wl-input-line))
                        gtk3wl-input-line))))
   (gtk3wl-input-line
    (if (not gtk3wl-select-overlay)
        (overlay-put (setq gtk3wl-select-overlay (make-overlay (point-min)
                                                           (point-min)))
                     'face 'highlight))
    (let ((beg (save-excursion
                 (goto-char (point-min))
                 (line-beginning-position
                  (if (consp gtk3wl-input-line)
                      (min (car gtk3wl-input-line) (cdr gtk3wl-input-line))
                    gtk3wl-input-line))))
          (end (save-excursion
                 (goto-char (point-min))
                 (line-beginning-position
                  (1+ (if (consp gtk3wl-input-line)
                          (max (car gtk3wl-input-line) (cdr gtk3wl-input-line))
                        gtk3wl-input-line))))))
      (move-overlay gtk3wl-select-overlay beg end)
      (deactivate-mark)
      (goto-char beg)))
   (t
    (if gtk3wl-select-overlay
        (setq gtk3wl-select-overlay (delete-overlay gtk3wl-select-overlay))))))

(defun gtk3wl-unselect-line ()
  "Removes any Nextstep highlight a buffer may contain."
  (if gtk3wl-select-overlay
      (setq gtk3wl-select-overlay (delete-overlay gtk3wl-select-overlay))))

(add-hook 'first-change-hook 'gtk3wl-unselect-line)

;;;; Preferences handling.
(declare-function gtk3wl-get-resource "gtk3wlfns.c" (owner name))

(defun get-lisp-resource (arg1 arg2)
  (let ((res (gtk3wl-get-resource arg1 arg2)))
    (cond
     ((not res) 'unbound)
     ((string-equal (upcase res) "YES") t)
     ((string-equal (upcase res) "NO")  nil)
     (t (read res)))))

;; gtk3wlterm.c

(declare-function gtk3wl-read-file-name "gtk3wlfns.c"
		  (prompt &optional dir mustmatch init dir_only_p))

;;;; File handling.

(defun x-file-dialog (prompt dir default_filename mustmatch only_dir_p)
"Read file name, prompting with PROMPT in directory DIR.
Use a file selection dialog.  Select DEFAULT-FILENAME in the dialog's file
selection box, if specified.  If MUSTMATCH is non-nil, the returned file
or directory must exist.

This function is only defined on GTK3WL, MS Windows, and X Windows with the
Motif or Gtk toolkits.  With the Motif toolkit, ONLY-DIR-P is ignored.
Otherwise, if ONLY-DIR-P is non-nil, the user can only select directories."
  (gtk3wl-read-file-name prompt dir mustmatch default_filename only_dir_p))

(defun gtk3wl-open-file-using-panel ()
  "Pop up open-file panel, and load the result in a buffer."
  (interactive)
  ;; Prompt dir defaultName isLoad initial.
  (setq gtk3wl-input-file (gtk3wl-read-file-name "Select File to Load" nil t nil))
  (if gtk3wl-input-file
      (and (setq gtk3wl-input-file (list gtk3wl-input-file)) (gtk3wl-find-file))))

(defun gtk3wl-write-file-using-panel ()
  "Pop up save-file panel, and save buffer in resulting name."
  (interactive)
  (let (gtk3wl-output-file)
    ;; Prompt dir defaultName isLoad initial.
    (setq gtk3wl-output-file (gtk3wl-read-file-name "Save As" nil nil nil))
    (message gtk3wl-output-file)
    (if gtk3wl-output-file (write-file gtk3wl-output-file))))

(defcustom gtk3wl-pop-up-frames 'fresh
  "Non-nil means open files upon request from the Workspace in a new frame.
If t, always do so.  Any other non-nil value means open a new frame
unless the current buffer is a scratch buffer."
  :type '(choice (const :tag "Never" nil)
                 (const :tag "Always" t)
                 (other :tag "Except for scratch buffer" fresh))
  :version "23.1"
  :group 'gtk3wl)

(declare-function gtk3wl-hide-emacs "gtk3wlfns.c" (on))

(defun gtk3wl-find-file ()
  "Do a `find-file' with the `gtk3wl-input-file' as argument."
  (interactive)
  (let* ((f (file-truename
	     (expand-file-name (pop gtk3wl-input-file)
			       command-line-default-directory)))
         (file (find-file-noselect f))
         (bufwin1 (get-buffer-window file 'visible))
         (bufwin2 (get-buffer-window "*scratch*" 'visible)))
    (cond
     (bufwin1
      (select-frame (window-frame bufwin1))
      (raise-frame (window-frame bufwin1))
      (select-window bufwin1))
     ((and (eq gtk3wl-pop-up-frames 'fresh) bufwin2)
      (gtk3wl-hide-emacs 'activate)
      (select-frame (window-frame bufwin2))
      (raise-frame (window-frame bufwin2))
      (select-window bufwin2)
      (find-file f))
     (gtk3wl-pop-up-frames
      (gtk3wl-hide-emacs 'activate)
      (let ((pop-up-frames t)) (pop-to-buffer file nil)))
     (t
      (gtk3wl-hide-emacs 'activate)
      (find-file f)))))


(defun gtk3wl-drag-n-drop (event &optional new-frame force-text)
  "Edit the files listed in the drag-n-drop EVENT.
Switch to a buffer editing the last file dropped."
  (interactive "e")
  (let* ((window (posn-window (event-start event)))
         (arg (car (cdr (cdr event))))
         (type (car arg))
         (data (car (cdr arg)))
         (url-or-string (cond ((eq type 'file)
                               (concat "file:" data))
                              (t data))))
    (set-frame-selected-window nil window)
    (when new-frame
      (select-frame (make-frame)))
    (raise-frame)
    (setq window (selected-window))
    (if force-text
        (dnd-insert-text window 'private data)
      (dnd-handle-one-url window 'private url-or-string))))


(defun gtk3wl-drag-n-drop-other-frame (event)
  "Edit the files listed in the drag-n-drop EVENT, in other frames.
May create new frames, or reuse existing ones.  The frame editing
the last file dropped is selected."
  (interactive "e")
  (gtk3wl-drag-n-drop event t))

(defun gtk3wl-drag-n-drop-as-text (event)
  "Drop the data in EVENT as text."
  (interactive "e")
  (gtk3wl-drag-n-drop event nil t))

(defun gtk3wl-drag-n-drop-as-text-other-frame (event)
  "Drop the data in EVENT as text in a new frame."
  (interactive "e")
  (gtk3wl-drag-n-drop event t t))

(global-set-key [drag-n-drop] 'gtk3wl-drag-n-drop)
(global-set-key [C-drag-n-drop] 'gtk3wl-drag-n-drop-other-frame)
(global-set-key [M-drag-n-drop] 'gtk3wl-drag-n-drop-as-text)
(global-set-key [C-M-drag-n-drop] 'gtk3wl-drag-n-drop-as-text-other-frame)

;;;; Frame-related functions.

;; gtk3wlterm.c
(defvar gtk3wl-alternate-modifier)
(defvar gtk3wl-right-alternate-modifier)
(defvar gtk3wl-right-command-modifier)
(defvar gtk3wl-right-control-modifier)

;; You say tomAYto, I say tomAHto..
(defvaralias 'gtk3wl-option-modifier 'gtk3wl-alternate-modifier)
(defvaralias 'gtk3wl-right-option-modifier 'gtk3wl-right-alternate-modifier)

(defun gtk3wl-do-hide-emacs ()
  (interactive)
  (gtk3wl-hide-emacs t))

(declare-function gtk3wl-hide-others "gtk3wlfns.c" ())

(defun gtk3wl-do-hide-others ()
  (interactive)
  (gtk3wl-hide-others))

(declare-function gtk3wl-emacs-info-panel "gtk3wlfns.c" ())

(defun gtk3wl-do-emacs-info-panel ()
  (interactive)
  (gtk3wl-emacs-info-panel))

(defun gtk3wl-next-frame ()
  "Switch to next visible frame."
  (interactive)
  (other-frame 1))

(defun gtk3wl-prev-frame ()
  "Switch to previous visible frame."
  (interactive)
  (other-frame -1))

;; Frame will be focused anyway, so select it
;; (if this is not done, mode line is dimmed until first interaction)
;; FIXME: Sounds like we're working around a bug in the underlying code.
(add-hook 'after-make-frame-functions 'select-frame)

(defvar tool-bar-mode)
(declare-function tool-bar-mode "tool-bar" (&optional arg))

;; Based on a function by David Reitter <dreitter@inf.ed.ac.uk> ;
;; see https://lists.gnu.org/archive/html/emacs-devel/2005-09/msg00681.html .
(defun gtk3wl-toggle-toolbar (&optional frame)
  "Switches the tool bar on and off in frame FRAME.
 If FRAME is nil, the change applies to the selected frame."
  (interactive)
  (modify-frame-parameters
   frame (list (cons 'tool-bar-lines
		       (if (> (or (frame-parameter frame 'tool-bar-lines) 0) 0)
				   0 1)) ))
  (if (not tool-bar-mode) (tool-bar-mode t)))


;;;; Dialog-related functions.

;; Ask user for confirm before printing.  Due to Kevin Rodgers.
(defun gtk3wl-print-buffer ()
  "Interactive front-end to `print-buffer': asks for user confirmation first."
  (interactive)
  (if (and (called-interactively-p 'interactive)
           (or (listp last-nonmenu-event)
               (and (char-or-string-p (event-basic-type last-command-event))
                    (memq 'super (event-modifiers last-command-event)))))
      (let ((last-nonmenu-event (if (listp last-nonmenu-event)
                                    last-nonmenu-event
                                  ;; Fake it:
                                  `(mouse-1 POSITION 1))))
        (if (y-or-n-p (format "Print buffer %s? " (buffer-name)))
            (print-buffer)
	  (error "Canceled")))
    (print-buffer)))

;;;; Font support.

;; Needed for font listing functions under both backend and normal
(setq scalable-fonts-allowed t)

;; Set to use font panel instead
(declare-function gtk3wl-popup-font-panel "gtk3wlfns.c" (&optional frame))
(defalias 'x-select-font 'gtk3wl-popup-font-panel "Pop up the font panel.
This function has been overloaded in Nextstep.")
(defalias 'mouse-set-font 'gtk3wl-popup-font-panel "Pop up the font panel.
This function has been overloaded in Nextstep.")

;; gtk3wlterm.c
(defvar gtk3wl-input-font)
(defvar gtk3wl-input-fontsize)

(defun gtk3wl-respond-to-change-font ()
  "Respond to changeFont: event, expecting `gtk3wl-input-font' and\n\
`gtk3wl-input-fontsize' of new font."
  (interactive)
  (modify-frame-parameters (selected-frame)
                           (list (cons 'fontsize gtk3wl-input-fontsize)))
  (modify-frame-parameters (selected-frame)
                           (list (cons 'font gtk3wl-input-font)))
  (set-frame-font gtk3wl-input-font))


;; Default fontset for macOS.  This is mainly here to show how a fontset
;; can be set up manually.  Ordinarily, fontsets are auto-created whenever
;; a font is chosen by
(defvar gtk3wl-standard-fontset-spec
  ;; Only some code supports this so far, so use uglier XLFD version
  ;; "-gtk3wl-*-*-*-*-*-10-*-*-*-*-*-fontset-standard,latin:Courier,han:Kai"
  (mapconcat 'identity
             '("-*-Monospace-*-*-*-*-10-*-*-*-*-*-fontset-standard"
               "latin:-*-Courier-*-*-*-*-10-*-*-*-*-*-iso10646-1"
               "han:-*-Kai-*-*-*-*-10-*-*-*-*-*-iso10646-1"
               "cyrillic:-*-Trebuchet$MS-*-*-*-*-10-*-*-*-*-*-iso10646-1")
             ",")
  "String of fontset spec of the standard fontset.
This defines a fontset consisting of the Courier and other fonts that
come with macOS.
See the documentation of `create-fontset-from-fontset-spec' for the format.")

(defvar gtk3wl-reg-to-script)               ; gtk3wlfont.c

;; This maps font registries (not exposed by GTK3WL APIs for font selection) to
;; Unicode scripts (which can be mapped to Unicode character ranges which are).
;; See ../international/fontset.el
(setq gtk3wl-reg-to-script
      '(("iso8859-1" . latin)
	("iso8859-2" . latin)
	("iso8859-3" . latin)
	("iso8859-4" . latin)
	("iso8859-5" . cyrillic)
	("microsoft-cp1251" . cyrillic)
	("koi8-r" . cyrillic)
	("iso8859-6" . arabic)
	("iso8859-7" . greek)
	("iso8859-8" . hebrew)
	("iso8859-9" . latin)
	("iso8859-10" . latin)
	("iso8859-11" . thai)
	("tis620" . thai)
	("iso8859-13" . latin)
	("iso8859-14" . latin)
	("iso8859-15" . latin)
	("iso8859-16" . latin)
	("viscii1.1-1" . latin)
	("jisx0201" . kana)
	("jisx0208" . han)
	("jisx0212" . han)
	("jisx0213" . han)
	("gb2312.1980" . han)
	("gb18030" . han)
	("gbk-0" . han)
	("big5" . han)
	("cns11643" . han)
	("sisheng_cwnn" . bopomofo)
	("ksc5601.1987" . hangul)
	("ethiopic-unicode" . ethiopic)
	("is13194-devanagari" . indian-is13194)
	("iso10646.indian-1" . devanagari)))


;;;; Pasteboard support.

(define-obsolete-function-alias 'gtk3wl-store-cut-buffer-internal
  'gui-set-selection "24.1")


(defun gtk3wl-copy-including-secondary ()
  (interactive)
  (call-interactively 'kill-ring-save)
  (gui-set-selection 'SECONDARY (buffer-substring (point) (mark t))))

(defun gtk3wl-paste-secondary ()
  (interactive)
  (insert (gui-get-selection 'SECONDARY)))


;;;; Scrollbar handling.

(global-set-key [vertical-scroll-bar down-mouse-1] 'scroll-bar-toolkit-scroll)
(global-set-key [horizontal-scroll-bar down-mouse-1] 'scroll-bar-toolkit-horizontal-scroll)
(global-unset-key [vertical-scroll-bar mouse-1])
(global-unset-key [vertical-scroll-bar drag-mouse-1])
(global-unset-key [horizontal-scroll-bar mouse-1])
(global-unset-key [horizontal-scroll-bar drag-mouse-1])


;;;; macOS-like defaults for trackpad and mouse wheel scrolling on
;;;; macOS 10.7+.

;; FIXME: This doesn't look right.  Is there a better way to do this
;; that keeps customize happy?
(when (featurep 'cocoa)
  (let ((appkit-version
         (progn (string-match "^appkit-\\([^\s-]*\\)" gtk3wl-version-string)
                (string-to-number (match-string 1 gtk3wl-version-string)))))
    ;; Appkit 1138 ~= macOS 10.7.
    (when (>= appkit-version 1138)
      (setq mouse-wheel-scroll-amount '(1 ((shift) . 5) ((control))))
      (put 'mouse-wheel-scroll-amount 'customized-value
           (list (custom-quote (symbol-value 'mouse-wheel-scroll-amount))))

      (setq mouse-wheel-progressive-speed nil)
      (put 'mouse-wheel-progressive-speed 'customized-value
           (list (custom-quote
                  (symbol-value 'mouse-wheel-progressive-speed)))))))


;;;; Color support.

;; Functions for color panel + drag
(defun gtk3wl-face-at-pos (pos)
  (let* ((frame (car pos))
         (frame-pos (cons (cadr pos) (cddr pos)))
         (window (window-at (car frame-pos) (cdr frame-pos) frame))
         (window-pos (coordinates-in-window-p frame-pos window))
         (buffer (window-buffer window))
         (edges (window-edges window)))
    (cond
     ((not window-pos)
      nil)
     ((eq window-pos 'mode-line)
      'mode-line)
     ((eq window-pos 'vertical-line)
      'default)
     ((consp window-pos)
      (with-current-buffer buffer
        (let ((p (car (compute-motion (window-start window)
                                      (cons (nth 0 edges) (nth 1 edges))
                                      (window-end window)
                                      frame-pos
                                      (- (window-width window) 1)
                                      nil
                                      window))))
          (cond
           ((eq p (window-point window))
            'cursor)
           ((and mark-active (< (region-beginning) p) (< p (region-end)))
            'region)
           (t
	    (let ((faces (get-char-property p 'face window)))
	      (if (consp faces) (car faces) faces)))))))
     (t
      nil))))

(defun gtk3wl-suspend-error ()
  ;; Don't allow suspending if any of the frames are GTK3WL frames.
  (if (memq 'gtk3wl (mapcar 'window-system (frame-list)))
      (error "Cannot suspend Emacs while an GTK3WL GUI frame exists")))


;; Set some options to be as Nextstep-like as possible.
(setq frame-title-format t
      icon-title-format t)


(defvar gtk3wl-initialized nil
  "Non-nil if Nextstep windowing has been initialized.")

(declare-function x-handle-args "common-win" (args))
(declare-function gtk3wl-list-services "gtk3wlfns.c" ())
(declare-function x-open-connection "gtk3wlfns.c"
                  (display &optional xrm-string must-succeed))
(declare-function gtk3wl-set-resource "gtk3wlfns.c" (owner name value))

;; Do the actual Nextstep Windows setup here; the above code just
;; defines functions and variables that we use now.
(cl-defmethod window-system-initialization (&context (window-system gtk3wl)
                                            &optional _display)
  "Initialize Emacs for Nextstep (Cocoa / GNUstep) windowing."
  (cl-assert (not gtk3wl-initialized))

  ;; PENDING: not needed?
  (setq command-line-args (x-handle-args command-line-args))

  ;; Setup the default fontset.
  (create-default-fontset)
  ;; Create the standard fontset.
  (condition-case err
      (create-fontset-from-fontset-spec gtk3wl-standard-fontset-spec t)
    (error (display-warning
            'initialization
            (format "Creation of the standard fontset failed: %s" err)
            :error)))

  (x-open-connection (system-name) x-command-line-resources t)

  ;; Add GNUstep menu items Services, Hide and Quit.  Rename Help to Info
  ;; and put it first (i.e. omit from menu-bar-final-items.
  (if (featurep 'gnustep)
      (progn
	(setq menu-bar-final-items '(buffer services hide-app quit))

	;; If running under GNUstep, "Help" is moved and renamed "Info".
	(bindings--define-key global-map [menu-bar help-menu]
	  (cons "Info" menu-bar-help-menu))
	(bindings--define-key global-map [menu-bar quit]
	  '(menu-item "Quit" save-buffers-kill-emacs
		      :help "Save unsaved buffers, then exit"))
	(bindings--define-key global-map [menu-bar hide-app]
	  '(menu-item "Hide" gtk3wl-do-hide-emacs
		      :help "Hide Emacs"))
	(bindings--define-key global-map [menu-bar services]
	  (cons "Services" (make-sparse-keymap "Services")))))


  (dolist (service (gtk3wl-list-services))
      (if (eq (car service) 'undefined)
	  (gtk3wl-define-service (cdr service))
	(define-key global-map (vector (car service))
	  (gtk3wl-define-service (cdr service)))))

  (if (and (eq (get-lisp-resource nil "NXAutoLaunch") t)
	   (eq (get-lisp-resource nil "HideOnAutoLaunch") t))
      (add-hook 'after-init-hook 'gtk3wl-do-hide-emacs))

  ;; FIXME: This will surely lead to "MODIFIED OUTSIDE CUSTOM" warnings.
  (menu-bar-mode (if (get-lisp-resource nil "Menus") 1 -1))

  ;; For Darwin nothing except UTF-8 makes sense.
  (when (eq system-type 'darwin)
      (add-hook 'before-init-hook
                #'(lambda ()
                    (setq locale-coding-system 'utf-8-unix)
                    (setq default-process-coding-system
                          '(utf-8-unix . utf-8-unix)))))

  ;; Mac OS X Lion introduces PressAndHold, which is unsupported by this port.
  ;; See this thread for more details:
  ;; https://lists.gnu.org/archive/html/emacs-devel/2011-06/msg00505.html
  (gtk3wl-set-resource nil "ApplePressAndHoldEnabled" "NO")

  (x-apply-session-resources)

  ;; Don't let Emacs suspend under GTK3WL.
  (add-hook 'suspend-hook 'gtk3wl-suspend-error)

  (setq gtk3wl-initialized t))

;; Any display name is OK.
(add-to-list 'display-format-alist '(".*" . gtk3wl))
(cl-defmethod handle-args-function (args &context (window-system gtk3wl))
  (x-handle-args args))

(cl-defmethod frame-creation-function (params &context (window-system gtk3wl))
  (x-create-frame-with-faces params))

(declare-function gtk3wl-own-selection-internal "gtk3wlselect.c" (selection value))
(declare-function gtk3wl-disown-selection-internal "gtk3wlselect.c" (selection))
(declare-function gtk3wl-selection-owner-p "gtk3wlselect.c" (&optional selection))
(declare-function gtk3wl-selection-exists-p "gtk3wlselect.c" (&optional selection))
(declare-function gtk3wl-get-selection "gtk3wlselect.c" (selection-symbol target-type))

(cl-defmethod gui-backend-set-selection (selection value
                                         &context (window-system gtk3wl))
  (if value (gtk3wl-own-selection-internal selection value)
    (gtk3wl-disown-selection-internal selection)))

(cl-defmethod gui-backend-selection-owner-p (selection
                                             &context (window-system gtk3wl))
  (gtk3wl-selection-owner-p selection))

(cl-defmethod gui-backend-selection-exists-p (selection
                                              &context (window-system gtk3wl))
  (gtk3wl-selection-exists-p selection))

(cl-defmethod gui-backend-get-selection (selection-symbol target-type
                                         &context (window-system gtk3wl))
  (gtk3wl-get-selection selection-symbol target-type))

(provide 'gtk3wl-win)
(provide 'term/gtk3wl-win)

;;; gtk3wl-win.el ends here
