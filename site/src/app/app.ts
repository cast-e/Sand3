import { Component, inject, signal } from '@angular/core';
import { RouterOutlet, RouterLink, RouterLinkActive } from '@angular/router';
import { FormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { AuthService } from './core/services/auth.service.js';

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [RouterOutlet, RouterLink, RouterLinkActive, FormsModule, CommonModule],
  templateUrl: './app.html',
  styleUrl: './app.scss'
})
export class App {
  auth = inject(AuthService);

  showAuthModal = signal(false);
  authMode = signal<'login' | 'register'>('login');
  authUsername = signal('');
  authPassword = signal('');
  authError = signal('');
  authLoading = signal(false);

  openAuthModal(mode: 'login' | 'register' = 'login') {
    this.authMode.set(mode);
    this.authUsername.set('');
    this.authPassword.set('');
    this.authError.set('');
    this.showAuthModal.set(true);
  }

  closeAuthModal() {
    this.showAuthModal.set(false);
  }

  switchAuthMode(mode: 'login' | 'register') {
    this.authMode.set(mode);
    this.authError.set('');
  }

  submitAuth() {
    const user = this.authUsername().trim();
    const pass = this.authPassword();

    if (!user || !pass) {
      this.authError.set('Please provide both username and password.');
      return;
    }

    this.authLoading.set(true);
    this.authError.set('');

    const op =
      this.authMode() === 'login'
        ? this.auth.login(user, pass)
        : this.auth.register(user, pass);

    op.subscribe({
      next: () => {
        this.authLoading.set(false);
        this.closeAuthModal();
      },
      error: (err) => {
        this.authLoading.set(false);
        const msg = err.error?.error || 'Authentication failed. Please try again.';
        this.authError.set(msg);
      }
    });
  }
}
